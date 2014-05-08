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
// parse_error::problem                                                                                               //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

parse_error::problem::problem(size_type line, size_type column, size_type character, std::string message) :
        _line(line),
        _column(column),
        _character(character),
        _message(std::move(message))
{ }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse_error                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string parse_error_what(const parse_error::problem_list& problems)
{
    std::ostringstream stream;
    bool first = true;
    for (const parse_error::problem& p : problems)
    {
        if (first)
            first = false;
        else
            stream << std::endl;
        
        stream << "On line " << p.line()
               << " column " << p.column()
               << " (char " << p.character() << "): "
               << p.message();
    }
    return stream.str();
}

parse_error::parse_error(problem_list problems, value partial_result) :
        std::runtime_error(parse_error_what(problems)),
        _problems(std::move(problems)),
        _partial_result(std::move(partial_result))
{ }

parse_error::~parse_error() throw()
{ }

const parse_error::problem_list& parse_error::problems() const
{
    return _problems;
}

const value& parse_error::partial_result() const
{
    return _partial_result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse_options                                                                                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

parse_options::parse_options() = default;

parse_options::~parse_options() throw() = default;

parse_options::on_error parse_options::failure_mode() const
{
    return _failure_mode;
}

parse_options& parse_options::failure_mode(on_error mode)
{
    _failure_mode = mode;
    return *this;
}

std::size_t parse_options::max_failures() const
{
    return _max_failures;
}

parse_options& parse_options::max_failures(std::size_t limit)
{
    _max_failures = limit;
    return *this;
}

parse_options::encoding parse_options::string_encoding() const
{
    return _string_encoding;
}

parse_options& parse_options::string_encoding(encoding encoding_)
{
    _string_encoding = encoding_;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse                                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail
{

struct JSONV_LOCAL parse_context
{
    typedef shared_buffer::size_type size_type;
    
    parse_options    options;
    string_decode_fn string_decode;
    
    size_type        line;
    size_type        column;
    size_type        character;
    const char*      input;
    size_type        input_size;
    char             current;
    bool             backed_off;
    
    bool                             successful;
    jsonv::parse_error::problem_list problems;
    
    explicit parse_context(const parse_options& options, const char* input, size_type length) :
            options(options),
            string_decode(get_string_decoder(options.string_encoding())),
            line(0),
            column(0),
            character(0),
            input(input),
            input_size(length),
            backed_off(false),
            successful(true)
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
            {
                ++column;
            }
            
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
    void parse_error(T&&... message)
    {
        std::ostringstream stream;
        parse_error_impl(stream, std::forward<T>(message)...);
    }
    
private:
    void parse_error_impl(std::ostringstream& stream)
    {
        jsonv::parse_error::problem problem(line, column, character, stream.str());
        if (options.failure_mode() == parse_options::on_error::fail_immediately)
        {
            throw jsonv::parse_error({ problem }, value(nullptr));
        }
        else
        {
            successful = false;
            if (problems.size() < options.max_failures())
                problems.emplace_back(std::move(problem));
        }
    }
    
    template <typename T, typename... TRest>
    void parse_error_impl(std::ostringstream& stream, T&& current, TRest&&... rest)
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
        context.parse_error("Could not extract ", kind_desc(is_double ? kind::decimal : kind::integer),
                            " from \"", characters, "\"");
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
        return context.string_decode(characters_start, characters_count);
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
    
    value arr = array();
    
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
    
    value obj = object();
    
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

value parse(const char* input, std::size_t length, const parse_options& options)
{
    detail::parse_context context(options, input, length);
    value out;
    if (!detail::parse_generic(context, out))
        context.parse_error("No input");
    
    if (context.successful || context.options.failure_mode() == parse_options::on_error::ignore)
        return out;
    else
        throw parse_error(context.problems, out);
}

value parse(std::istream& input, const parse_options& options)
{
    // Copy the input into a buffer
    input.seekg(0, std::ios::end);
    std::size_t len = input.tellg();
    detail::shared_buffer buffer(len);
    input.seekg(0, std::ios::beg);
    std::copy(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>(),
              buffer.get_mutable(0, len)
             );
    
    return parse(buffer.cbegin(), buffer.size(), options);
}

value parse(const std::string& source, const parse_options& options)
{
    return parse(source.c_str(), source.size(), options);
}

}
