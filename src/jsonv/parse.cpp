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
#include <jsonv/encode.hpp>
#include <jsonv/parse_index.hpp>
#include <jsonv/serialization.hpp>

#include "char_convert.hpp"

#include <istream>
#include <sstream>
#include <streambuf>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse_error                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string parse_error_what(const char* message, optional<std::size_t> character)
{
    std::ostringstream os;

    if (character)
        os << "At " << *character << ": ";
    os << message;

    return std::move(os).str();
}

parse_error::parse_error(const char* message, optional<std::size_t> character) noexcept :
        std::runtime_error(parse_error_what(message, character)),
        _character(character)
{ }

parse_error::parse_error(const char* message) noexcept :
        std::runtime_error(message)
{ }

parse_error::~parse_error() noexcept
{ }

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
           .string_encoding(encoding::utf8_strict)
           .max_structure_depth(20)
           .require_document(true)
           .complete_parse(true)
           .comments(false)
           ;
}

parse_options& parse_options::string_encoding(encoding encoding_)
{
    _string_encoding = encoding_;
    return *this;
}

parse_options& parse_options::max_structure_depth(optional<size_type> depth)
{
    _max_struct_depth = depth;
    return *this;
}

parse_options& parse_options::require_document(bool val)
{
    _require_document = val;
    return *this;
}

parse_options& parse_options::complete_parse(bool complete_parse_)
{
    _complete_parse = complete_parse_;
    return *this;
}

parse_options& parse_options::comments(bool val)
{
    _comments = val;
    return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// parse functions                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define JSONV_IDENTITY(...) __VA_ARGS__

#define JSONV_PARSE_IMPL_OVERLOADS(params, args)                                                                       \
    value parse(JSONV_IDENTITY params)                                                                                 \
    {                                                                                                                  \
        return parse(JSONV_IDENTITY args, parse_options::create_default(), extract_options::create_default());         \
    }                                                                                                                  \
                                                                                                                       \
    value parse(JSONV_IDENTITY params, const parse_options& parse_opts)                                                \
    {                                                                                                                  \
        return parse(JSONV_IDENTITY args, parse_opts, extract_options::create_default());                              \
    }                                                                                                                  \
                                                                                                                       \
    value parse(JSONV_IDENTITY params, const extract_options& extract_opts)                                            \
    {                                                                                                                  \
        return parse(JSONV_IDENTITY args, parse_options::create_default(), extract_opts);                              \
    }                                                                                                                  \

value parse(string_view input, const parse_options& parse_opts, const extract_options& extract_opts)
{
    auto ast = parse_index::parse(input, parse_opts);
    ast.validate();
    return ast.extract_tree(extract_opts);
}

JSONV_PARSE_IMPL_OVERLOADS((string_view input), (input))

value parse(std::istream& input, const parse_options& parse_opts, const extract_options& extract_opts)
{
    auto text = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    return parse(text, parse_opts, extract_opts);
}

JSONV_PARSE_IMPL_OVERLOADS((std::istream& input), (input))

value parse(const char* begin, const char* end, const parse_options& parse_opts, const extract_options& extract_opts)
{
    return parse(string_view(begin, std::distance(begin, end)), parse_opts, extract_opts);
}

JSONV_PARSE_IMPL_OVERLOADS((const char* begin, const char* end), (begin, end))

value operator"" _json(const char* str, std::size_t len)
{
    return parse(string_view(str, len));
}

}
