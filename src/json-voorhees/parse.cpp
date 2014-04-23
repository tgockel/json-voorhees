/** \file
 *  
 *  Copyright (c) 2012-2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <json-voorhees/parse.hpp>
#include <json-voorhees/array.hpp>
#include <json-voorhees/object.hpp>

#include "char_convert.hpp"

#include <cassert>
#include <cctype>
#include <set>
#include <sstream>

#include <boost/lexical_cast.hpp>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse_error                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string parse_error_what(unsigned line, unsigned column, unsigned character, const std::string& message)
{
    std::ostringstream stream;
    stream << "On line " << line << " column " << column << " (char " << character << "): " << message;
    return stream.str();
}

parse_error::parse_error(unsigned line_, unsigned column_, unsigned character_, const std::string& message_) :
        std::runtime_error(parse_error_what(line_, column_, character_, message_)),
        _line(line_),
        _column(column_),
        _character(character_),
        _message(message_)
{ }

parse_error::~parse_error() throw()
{ }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse                                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail
{

struct parse_context
{
    unsigned      line;
    unsigned      column;
    unsigned      character;
    istream_type& input;
    char_type     current;
    bool          backed_off;
    
    explicit parse_context(istream_type& input) :
            line(0),
            column(0),
            character(0),
            input(input),
            backed_off(false)
    { }
    
    parse_context(const parse_context&) = delete;
    parse_context& operator=(const parse_context&) = delete;
    
    bool next()
    {
        if (backed_off)
        {
            backed_off = false;
            return true;
        }
        
        input.get(current);
        if (input.gcount() > 0)
        {
            ++character;
            if (current == '\n' || current == '\r')
            {
                ++line;
                column = 0;
            }
            else
                ++column;
            
            return true;
        }
        else
            return false;
    }
    
    void previous()
    {
        if (backed_off)
            parse_error("Internal parser error");
        backed_off = true;
    }
    
    template <typename... T>
    void parse_error(T&&... message) const
    {
        std::ostringstream stream;
        parse_error_impl(stream, std::forward<T>(message)...);
    }
    
private:
    void parse_error_impl(std::ostringstream& stream) const
    {
        throw jsonv::parse_error(line, column, character, stream.str());
    }
    
    template <typename T, typename... TRest>
    void parse_error_impl(std::ostringstream& stream, T&& current, TRest&&... rest) const
    {
        stream << std::forward<T>(current);
        parse_error_impl(stream, std::forward<TRest>(rest)...);
    }
};

static bool parse_generic(parse_context& context, value& out, bool eat_whitespace = true);

static bool eat_whitespace(parse_context& context)
{
    while (context.next())
    {
        if (!isspace(context.current))
            return true;
    }
    return false;
}

static value parse_literal(parse_context& context, const value& outval, const char_type* const literal)
{
    assert(context.current == *literal);
    
    for (const char_type* ptr = literal; *ptr; ++ptr)
    {
        if (context.current != *ptr)
            context.parse_error("Unexpected character '", context.current, "' while trying to match ", literal);
        
        if (!context.next())
            context.parse_error("Unexpected end while trying to match ", literal);
    }
    
    return outval;
}

static value parse_null(parse_context& context)
{
    return parse_literal(context, value(), "null");
}

static value parse_true(parse_context& context)
{
    return parse_literal(context, true, "true");
}

static value parse_false(parse_context& context)
{
    return parse_literal(context, false, "false");
}

static std::set<char_type> get_allowed_number_chars()
{
    const string_type allowed_chars_src("-0123456789+-eE.");
    return std::set<char_type>(allowed_chars_src.begin(), allowed_chars_src.end());
}

static value parse_number(parse_context& context)
{
    // Regex is probably the more "right" way to get a number, but lexical_cast is faster and easier.
    //typedef std::basic_regex<char_type> regex;
    //static const regex number_pattern("^(-)?(0|[1-9][0-9]*)(?:(\\.)([0-9]+))?(?:([eE])([+-])?([0-9]+))?$");
                                     // 0   |1                |2   |3           |4    |5     |6
                                     // sign|whole            |dot |decimal     |ise  |esign |exp
                                     // re.compile("^(-)?(0|[1-9][0-9]*)(?:(\\.)([0-9]+))?(?:([eE])([+-])?([0-9]+))?$") \.
                                     //   .match('-123.45e+67').groups()
                                     // ('-', '123', '.', '45', 'e', '+', '67')
    static std::set<char_type> allowed_chars = get_allowed_number_chars();
    string_type characters(1, context.current);
    bool is_double = false;
    while (true)
    {
        if (!context.next())
            // it is okay to end the stream in the middle of a number
            break;
        
        if (!allowed_chars.count(context.current))
        {
            context.previous();
            break;
        }
        else
        {
            if (context.current == '.')
                is_double = true;
            
            characters += context.current;
        }
    }
    
    try
    {
        if (is_double)
            return boost::lexical_cast<double>(characters);
        else if (characters[0] == '-')
            return boost::lexical_cast<int64_t>(characters);
        else
            return static_cast<int64_t>(boost::lexical_cast<uint64_t>(characters));
    }
    catch (boost::bad_lexical_cast&)
    {
        context.parse_error("Could not extract ", is_double ? "real" : "integer", " from \"", characters, "\"");
        return value();
    }
}

static string_type parse_string(parse_context& context)
{
    assert(context.current == '\"');
    
    string_type characters;
    
    while (true)
    {
        if (!context.next())
        {
            context.parse_error("Unterminated string \"", characters);
            break;
        }
        
        if (context.current == '\"')
        {
            if (characters.size() > 0 && characters[characters.size()-1] == '\\'
                && !(characters.size() > 1 && characters[characters.size()-2] == '\\') //already escaped
               )
            {
                characters += context.current;
            }
            else
                break;
        }
        else
        {
            characters += context.current;
        }
    }
    
    try
    {
        return string_decode(characters);
    }
    catch (const detail::decode_error& err)
    {
        context.parse_error("Error decoding string:", err.what());
        // return it un-decoded
        return characters;
    }
}

static value parse_array(parse_context& context)
{
    assert(context.current == '[');
    
    array arr;
    
    while (true)
    {
        if (!eat_whitespace(context))
        {
            context.parse_error("Unexpected end: unmatched '['");
            break;
        }
        
        if (context.current == ']')
        {
            break;
        }
        else if (context.current == ',')
            continue;
        else
        {
            value val;
            if (parse_generic(context, val, false))
            {
                arr.push_back(val);
            }
            else
            {
                context.parse_error("Unexpected end: unmatched '['");
                break;
            }
        }
    }
    return arr;
}

static value parse_object(parse_context& context)
{
    assert(context.current == '{');
    
    object obj;
    
    while (true)
    {
        if (!eat_whitespace(context))
            context.parse_error("Unexpected end: unmatched '{'");
        
        switch (context.current)
        {
        case ',':
            // Jump over all commas in a row.
            break;
        case '\"':
        {
            string_type key = parse_string(context);
            if (!eat_whitespace(context))
                context.parse_error("Unexpected end: missing ':' for key '", key, "'");
            
            if (context.current != ':')
                context.parse_error("Invalid character '", context.current, "' expecting ':'");
            
            value val;
            if (!parse_generic(context, val))
                context.parse_error("Unexpected end: missing value for key '", key, "'");
            
            //object::iterator iter = obj.find(key);
            //if (iter != obj.end())
            //    context.parse_error("Duplicate key \"", key, "\"");
            obj[key] = val;
            break;
        }
        case '}':
            context.previous();
            return obj;
        default:
            context.parse_error("Invalid character '", context.current, "' expecting '}' or a key (string)");
            return obj;
        }
    }
}

static bool parse_generic(parse_context& context, value& out, bool eat_whitespace_)
{
    if (eat_whitespace_)
        if (!eat_whitespace(context))
            return false;
    
    switch (context.current)
    {
    case '{':
        out = parse_object(context);
        return true;
    case '[':
        out = parse_array(context);
        return true;
    case '\"':
        out = parse_string(context);
        return true;
    case 'n':
        out = parse_null(context);
        return true;
    case 't':
        out = parse_true(context);
        return true;
    case 'f':
        out = parse_false(context);
        return true;
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        out = parse_number(context);
        return true;
    default:
        context.parse_error("Invalid character '", context.current, "'");
        return true;
    }
}

}

value parse(istream_type& input)
{
    detail::parse_context context(input);
    value out;
    if (detail::parse_generic(context, out))
        return out;
    else
    {
        context.parse_error("No input");
        return out;
    }
}

value parse(const string_type& source)
{
    std::basic_istringstream<char_type> input(source);
    return parse(input);
}

}
