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
#include <jsonv/parse.hpp>
#include <jsonv/array.hpp>
#include <jsonv/object.hpp>
#include <jsonv/detail/shared_buffer.hpp>

#include "char_convert.hpp"

#include <cassert>
#include <cctype>
#include <istream>
#include <set>
#include <sstream>

#include <boost/lexical_cast.hpp>

#if 0
#   include <iostream>
#   define JSONV_DBG_NEXT(x)   std::cout << x
#   define JSONV_DBG_STRUCT(x) std::cout << "\033[0;32m" << x << "\033[m"
#else
#   define JSONV_DBG_NEXT(x)
#   define JSONV_DBG_STRUCT(x)
#endif

namespace jsonv
{


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse_error                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string parse_error_what(std::size_t line,
                                    std::size_t column,
                                    std::size_t character,
                                    const std::string& message
                                   )
{
    std::ostringstream stream;
    stream << "On line " << line << " column " << column << " (char " << character << "): " << message;
    return stream.str();
}

parse_error::parse_error(size_type line_, size_type column_, size_type character_, const std::string& message_) :
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
    typedef shared_buffer::size_type size_type;
    
    size_type        line;
    size_type        column;
    size_type        character;
    const char*      input;
    size_type        input_size;
    char             current;
    bool             backed_off;
    
    explicit parse_context(const char* input, size_type length) :
            line(0),
            column(0),
            character(0),
            input(input),
            input_size(length),
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
        
        if (character < input_size)
        {
            current = input[character];
            JSONV_DBG_NEXT(current);
            
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

static value parse_literal(parse_context& context, const value& outval, const char* const literal)
{
    assert(context.current == *literal);
    
    for (const char* ptr = literal; *ptr; ++ptr)
    {
        if (context.current != *ptr)
            context.parse_error("Unexpected character '", context.current, "' while trying to match ", literal);
        
        if (!context.next() && ptr[1])
            context.parse_error("Unexpected end while trying to match ", literal);
    }
    
    context.previous();
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

static std::set<char> get_allowed_number_chars()
{
    const std::string allowed_chars_src("-0123456789+-eE.");
    return std::set<char>(allowed_chars_src.begin(), allowed_chars_src.end());
}

static value parse_number(parse_context& context)
{
    // Regex is probably the more "right" way to get a number, but lexical_cast is faster and easier.
    //typedef std::regex regex;
    //static const regex number_pattern("^(-)?(0|[1-9][0-9]*)(?:(\\.)([0-9]+))?(?:([eE])([+-])?([0-9]+))?$");
                                     // 0   |1                |2   |3           |4    |5     |6
                                     // sign|whole            |dot |decimal     |ise  |esign |exp
                                     // re.compile("^(-)?(0|[1-9][0-9]*)(?:(\\.)([0-9]+))?(?:([eE])([+-])?([0-9]+))?$") \.
                                     //   .match('-123.45e+67').groups()
                                     // ('-', '123', '.', '45', 'e', '+', '67')
    static std::set<char> allowed_chars = get_allowed_number_chars();
    const char* characters_start = context.input + context.character - 1;
    std::size_t characters_count = 1;
    
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
            
            ++characters_count;
        }
    }
    
    try
    {
        
        if (is_double)
            return boost::lexical_cast<double>(characters_start, characters_count);
        else if (characters_start[0] == '-')
            return boost::lexical_cast<int64_t>(characters_start, characters_count);
        else
            return static_cast<int64_t>(boost::lexical_cast<uint64_t>(characters_start, characters_count));
    }
    catch (boost::bad_lexical_cast&)
    {
        std::string characters(characters_start, characters_count);
        context.parse_error("Could not extract ", is_double ? "real" : "integer", " from \"", characters, "\"");
        return value();
    }
}

static std::string parse_string(parse_context& context)
{
    assert(context.current == '\"');
    
    const char* characters_start = context.input + context.character;
    std::size_t characters_count = 0;
    
    while (true)
    {
        if (!context.next())
        {
            context.parse_error("Unterminated string \"", std::string(characters_start, characters_count));
            break;
        }
        
        if (context.current == '\"')
        {
            if (characters_count > 0 && characters_start[characters_count-1] == '\\'
                && !(characters_count > 1 && characters_start[characters_count-2] == '\\') //already escaped
               )
            {
                ++characters_count;
            }
            else
                break;
        }
        else
        {
            ++characters_count;
        }
    }
    
    try
    {
        return string_decode(characters_start, characters_count);
    }
    catch (const detail::decode_error& err)
    {
        context.parse_error("Error decoding string:", err.what());
        // return it un-decoded
        return std::string(characters_start, characters_count);
    }
}

static value parse_array(parse_context& context)
{
    assert(context.current == '[');
    JSONV_DBG_STRUCT('[');
    
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
    JSONV_DBG_STRUCT(']');
    return arr;
}

static value parse_object(parse_context& context)
{
    assert(context.current == '{');
    JSONV_DBG_STRUCT('{');
    
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
            JSONV_DBG_STRUCT('(');
            std::string key = parse_string(context);
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
            JSONV_DBG_STRUCT(')');
            break;
        }
        case '}':
            JSONV_DBG_STRUCT('}');
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

value parse(const char* input, std::size_t length)
{
    detail::parse_context context(input, length);
    value out;
    if (detail::parse_generic(context, out))
    {
        return out;
    }
    else
    {
        context.parse_error("No input");
        return out;
    }
}

value parse(std::istream& input)
{
    // Copy the input into a buffer
    input.seekg(0, std::ios::end);
    std::size_t len = input.tellg();
    detail::shared_buffer buffer(len);
    input.seekg(0, std::ios::beg);
    std::copy(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>(),
              buffer.get_mutable(0, len)
             );
    
    return parse(buffer.cbegin(), buffer.size());
}

value parse(const std::string& source)
{
    return parse(source.c_str(), source.size());
}

}
