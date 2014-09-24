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
#include <jsonv/tokenizer.hpp>

#include "char_convert.hpp"

#include <cassert>
#include <cctype>
#include <istream>
#include <set>
#include <sstream>
#include <vector>

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

parse_error::~parse_error() noexcept
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

parse_options::~parse_options() noexcept = default;

parse_options parse_options::create_default()
{
    return parse_options();
}

parse_options parse_options::create_strict()
{
    return parse_options()
           .failure_mode(on_error::fail_immediately)
           .string_encoding(encoding::utf8)
           .require_document(true)
           .complete_parse(true)
           ;
}

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

bool parse_options::require_document() const
{
    return _require_document;
}

parse_options& parse_options::require_document(bool val)
{
    _require_document = val;
    return *this;
}

bool parse_options::complete_parse() const
{
    return _complete_parse;
}

parse_options& parse_options::complete_parse(bool complete_parse_)
{
    _complete_parse = complete_parse_;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parsing internals                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail
{

struct JSONV_LOCAL parse_context
{
    using size_type = std::size_t;
    
    tokenizer&       input;
    parse_options    options;
    string_decode_fn string_decode;
    
    size_type line;
    size_type column;
    size_type character;
    
    bool                             successful;
    jsonv::parse_error::problem_list problems;
    bool                             complete;
    
    explicit parse_context(const parse_options& options, tokenizer& input) :
            input(input),
            options(options),
            string_decode(get_string_decoder(options.string_encoding())),
            line(0),
            column(1),
            character(0),
            successful(true),
            problems(),
            complete(false)
    { }
    
    parse_context(const parse_context&) = delete;
    parse_context& operator=(const parse_context&) = delete;
    
    bool next()
    {
        if (!complete && line != 0)
        {
            character += current().text.size();
            for (const char c : current().text)
            {
                if (c == '\n' || c == '\r')
                {
                    ++line;
                    column = 1;
                }
                else
                {
                    ++column;
                }
            }
        }
        else
        {
            ++line;
        }
        
        if (input.next())
        {
            JSONV_DBG_NEXT("(" << input.current().text << " cxt:" << input.current().kind << ")");
            if (current_kind() == token_kind::whitespace)
                return next();
            else
                return true;
        }
        else
        {
            complete = true;
            return false;
        }
    }
    
    const tokenizer::token& current() const
    {
        return input.current();
    }
    
    const token_kind& current_kind() const
    {
        return current().kind;
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
        try
        {
            string_ref text = current().text;
            stream << ": \"" << text << "\"";
        }
        catch (const std::logic_error&)
        { }
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

static bool parse_generic(parse_context& context, value& out, bool advance = true);

static bool parse_boolean(parse_context& context, value& out)
{
    assert(context.current_kind() == token_kind::boolean);
    switch (context.current().text.at(0))
    {
    case 't':
        out = true;
        return true;
    case 'f':
        out = false;
        return true;
    default:
        context.parse_error("Invalid token for boolean.");
        return true;
    }
}

static bool parse_null(parse_context& context, value& out)
{
    assert(context.current_kind() == token_kind::null);
    out = nullptr;
    return true;
}

static bool parse_number(parse_context& context, value& out)
{
    JSONV_DBG_STRUCT("#");
    string_ref characters = context.current().text;
    try
    {
        
        if (characters[0] == '-')
            out = boost::lexical_cast<int64_t>(characters.data(), characters.size());
        else
            // For non-negative integer types, use lexical_cast of a uint64_t then static_cast to an int64_t. This is
            // done to deal with the values 2^63..2^64-1 -- do not consider it an exception, as we can store the bits
            // properly, but the onus is on the user to know the particular key was in the overflow range.
            out = static_cast<int64_t>(boost::lexical_cast<uint64_t>(characters.data(), characters.size()));
    }
    catch (boost::bad_lexical_cast&)
    {
        // could not get an integer...try to get a double
        try
        {
            out = boost::lexical_cast<double>(characters.data(), characters.size());
        }
        catch (boost::bad_lexical_cast&)
        {
            context.parse_error("Could not extract number from \"", characters, "\"");
            out = nullptr;
        }
    }
    return true;
}

static std::string parse_string(parse_context& context)
{
    assert(context.current_kind() == token_kind::string);
    
    string_ref source = context.current().text;
    JSONV_DBG_STRUCT(source);
    // chop off the ""s
    source.remove_prefix(1);
    source.remove_suffix(1);
    
    try
    {
        return context.string_decode(source);
    }
    catch (const detail::decode_error& err)
    {
        context.parse_error("Error decoding string:", err.what());
        // return it un-decoded
        return std::string(source);
    }
}

static bool parse_string(parse_context& context, value& out)
{
    out = parse_string(context);
    return true;
}

static bool parse_array(parse_context& context, value& arr)
{
    JSONV_DBG_STRUCT('[');
    arr = array();
    
    while (true)
    {
        if (!context.next())
            break;
        
        value val;
        if (context.current_kind() == token_kind::array_end)
        {
            JSONV_DBG_STRUCT(']');
            return true;
        }
        else if (parse_generic(context, val, false))
        {
            JSONV_DBG_STRUCT(val);
            arr.push_back(std::move(val));
        }
        else
        {
            JSONV_DBG_STRUCT("parse error:" << context.current().text << " kind:" << context.current_kind());
            // a parse error, but parse_generic will have complained about it
        }
        
        if (!context.next())
        {
            break;
        }
        
        if (context.current_kind() == token_kind::array_end)
        {
            JSONV_DBG_STRUCT(']');
            return true;
        }
        else if (context.current_kind() == token_kind::separator)
        {
            JSONV_DBG_STRUCT(',');
        }
        else
        {
            context.parse_error("Invalid entry when looking for ',' or ']'");
        }
    }
    context.parse_error("Unexpected end: unmatched '['");
    return false;
}

static bool parse_object(parse_context& context, value& out)
{
    out = object();
    
    while (context.next())
    {
        std::string key;
        if (context.current_kind() == token_kind::string)
        {
            key = parse_string(context);
        }
        else if (context.current_kind() == token_kind::object_end)
        {
            return true;
        }
        else
        {
            context.parse_error("Expecting a key, but found ", context.current_kind());
            // simulate a new key
            key = std::string(context.current().text);
        }
        
        if (!context.next())
        {
            context.parse_error("Unexpected end: missing ':' for key '", key, "'");
            return false;
        }
        
        if (context.current_kind() != token_kind::object_key_delimiter)
            context.parse_error("Invalid key-value delimiter...expecting ':' after key '", key, "'");
        
        value val;
        if (!parse_generic(context, val))
        {
            context.parse_error("Unexpected end: incomplete value for key '", key, "'");
            return false;
        }
        
        auto iter = out.find(key);
        if (iter == out.end_object())
        {
            out.insert({ std::move(key), std::move(val) });
        }
        else
        {
            context.parse_error("Duplicate entries for key '", key, "'. ",
                                "Updating old value ", iter->second, " with new value ", val, "."
                               );
            iter->second = std::move(val);
        }
        
        if (!context.next())
            break;
        
        if (context.current_kind() == token_kind::object_end)
            return true;
        else if (context.current_kind() != token_kind::separator)
            context.parse_error("Invalid token while searching for next value in object.");
    }
    
    context.parse_error("Unexpected end inside of object.");
    return false;
}

static bool parse_generic(parse_context& context, value& out, bool advance)
{
    if (advance && !context.next())
        return false;
    
    switch (context.current().kind)
    {
    case token_kind::array_begin:
        return parse_array(context, out);
    case token_kind::boolean:
        return parse_boolean(context, out);
    case token_kind::null:
        return parse_null(context, out);
    case token_kind::number:
        return parse_number(context, out);
    case token_kind::object_begin:
        return parse_object(context, out);
    case token_kind::string:
        return parse_string(context, out);
    case token_kind::comment:
    case token_kind::whitespace:
        // ignore
        return parse_generic(context, out);
    case token_kind::unknown:
    case token_kind::array_end:
    case token_kind::object_end:
    case token_kind::object_key_delimiter:
    case token_kind::separator:
    case token_kind::parse_error_indicator:
        context.parse_error("Encountered invalid token ", context.current().kind, ": \"", context.current().text, "\"");
    }
    return true;
}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse functions                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static value post_parse(detail::parse_context& context, value&& out_)
{
    // allow RVO
    value out(std::move(out_));
    if (context.successful && context.options.complete_parse())
    {
        while (context.next())
        {
            if (  context.current_kind() != token_kind::whitespace
               && context.current_kind() != token_kind::comment
               && context.current_kind() != token_kind::unknown
               )
            {
                context.parse_error("Found non-trivial data after final token. ", context.current_kind());
            }
        }
    }
    
    if (context.successful && context.options.require_document())
    {
        if (out.get_kind() != kind::array && out.get_kind() != kind::object)
        {
            context.parse_error("JSON requires the root of a payload to be an array or object, not ", out.get_kind());
        }
    }
    
    if (context.successful || context.options.failure_mode() == parse_options::on_error::ignore)
        return out;
    else
        throw parse_error(context.problems, out);
}

value parse(tokenizer& input, const parse_options& options)
{
    detail::parse_context context(options, input);
    value out;
    if (!detail::parse_generic(context, out))
        context.parse_error("No input");
    
    return post_parse(context, std::move(out));
}

value parse(std::istream& input, const parse_options& options)
{
    tokenizer tokens(input);
    return parse(tokens, options);
}

value parse(const char* input, std::size_t length, const parse_options& options)
{
    std::stringstream sstream;
    sstream.write(input, length);
    return parse(sstream, options);
}


value parse(const std::string& source, const parse_options& options)
{
    std::istringstream istream(source);
    return parse(istream, options);
}

}
