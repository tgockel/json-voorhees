/// \file
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/parse.hpp>
#include <jsonv/array.hpp>
#include <jsonv/encode.hpp>
#include <jsonv/object.hpp>

#include "char_convert.hpp"

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <istream>
#include <set>
#include <sstream>
#include <streambuf>
#include <vector>

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

std::ostream& operator<<(std::ostream& os, const parse_error::problem& p)
{
    return os << "At line " << p.line() << ':' << p.column() << " (char " << p.character() << "): " << p.message();
}

std::string to_string(const parse_error::problem& p)
{
    std::ostringstream os;
    os << p;
    return os.str();
}

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

        stream << p;
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

std::ostream& operator<<(std::ostream& os, const parse_error& p)
{
    return os << p.what();
}

std::string to_string(const parse_error& p)
{
    std::ostringstream os;
    os << p;
    return os.str();
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
           .string_encoding(encoding::utf8_strict)
           .number_encoding(numbers::strict)
           .comma_policy(commas::strict)
           .max_structure_depth(20)
           .require_document(true)
           .complete_parse(true)
           .comments(false)
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

parse_options::numbers parse_options::number_encoding() const
{
    return _number_encoding;
}

parse_options& parse_options::number_encoding(numbers encoding_)
{
    _number_encoding = encoding_;
    return *this;
}

parse_options::commas parse_options::comma_policy() const
{
    return _comma_policy;
}

parse_options& parse_options::comma_policy(commas policy)
{
    _comma_policy = policy;
    return *this;
}

parse_options::size_type parse_options::max_structure_depth() const
{
    return _max_struct_depth;
}

parse_options& parse_options::max_structure_depth(size_type depth)
{
    _max_struct_depth = depth;
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

bool parse_options::comments() const
{
    return _comments;
}

parse_options& parse_options::comments(bool val)
{
    _comments = val;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse functions                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

value parse(std::istream& input, const parse_options& options)
{
    auto text = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return parse(text, options);
}

value parse(const char* begin, const char* end, const parse_options& options)
{
    return parse(string_view(begin, std::distance(begin, end)), options);
}

value operator"" _json(const char* str, std::size_t len)
{
    return parse(string_view(str, len));
}

}
