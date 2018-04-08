/** \file
 *  
 *  Copyright (c) 2014-2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/tokenizer.hpp>
#include <jsonv/detail/token_patterns.hpp>

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <istream>
#include <iterator>
#include <sstream>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// token_kind                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const token_kind& value)
{
    static constexpr token_kind all_valid_tokens = token_kind(0x0fff);
    static constexpr token_kind non_error_tokens = token_kind(0xffff);
    
    switch (value)
    {
    case token_kind::unknown:                return os << "unknown";
    case token_kind::array_begin:            return os << '[';
    case token_kind::array_end:              return os << ']';
    case token_kind::boolean:                return os << "boolean";
    case token_kind::null:                   return os << "null";
    case token_kind::number:                 return os << "number";
    case token_kind::separator:              return os << ',';
    case token_kind::string:                 return os << "string";
    case token_kind::object_begin:           return os << '{';
    case token_kind::object_key_delimiter:   return os << ':';
    case token_kind::object_end:             return os << '}';
    case token_kind::whitespace:             return os << "whitespace";
    case token_kind::comment:                return os << "comment";
    case token_kind::parse_error_indicator:
    default:
        // if the value represents a parse error...
        if ((value & token_kind::parse_error_indicator) == token_kind::parse_error_indicator)
        {
            return os << "parse_error("
                      << (value & all_valid_tokens)
                      << ')';
        }
        // not a parse error
        else
        {
            token_kind post = value & non_error_tokens;
            for (token_kind scan_token = static_cast<token_kind>(1);
                 bool(post);
                 scan_token = token_kind(static_cast<unsigned int>(scan_token) << 1)
                )
            {
                if (bool(scan_token & post))
                {
                    if (bool(scan_token & all_valid_tokens))
                        os << scan_token;
                    else
                        os << std::hex << "0x" << std::setfill('0') << std::setw(4)
                           << static_cast<unsigned int>(scan_token)
                           << std::dec << std::setfill(' ');
                    
                    post = post & ~scan_token;
                    if (bool(post))
                        os << '|';
                }
            }
            return os;
        }
    }
}

std::string to_string(const token_kind& value)
{
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// tokenizer                                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static tokenizer::size_type& min_buffer_size_ref()
{
    static tokenizer::size_type instance = 1024 * sizeof(void*);
    return instance;
}

tokenizer::size_type tokenizer::min_buffer_size()
{
    return min_buffer_size_ref();
}

void tokenizer::set_min_buffer_size(tokenizer::size_type sz)
{
    min_buffer_size_ref() = std::max(sz, tokenizer::size_type(1));
}

tokenizer::tokenizer(string_view input) :
        _input(input),
        _position(_input.data())
{ }

tokenizer::tokenizer(std::shared_ptr<std::string> input) :
        _input(*input),
        _position(_input.data()),
        _track(std::move(input))
{ }

static std::string load_single_string(std::istream& input)
{
    std::string out;
    char buffer[16 * 1024];

    while (input.read(buffer, sizeof buffer))
    {
        out.append(buffer, sizeof buffer);
    }
    out.append(buffer, input.gcount());

    return out;
}

tokenizer::tokenizer(std::istream& input) :
        tokenizer(std::make_shared<std::string>(load_single_string(input)))
{ }

tokenizer::~tokenizer() noexcept
{ }

const string_view& tokenizer::input() const
{
    return _input;
}

const tokenizer::token& tokenizer::current() const
{
    if (_current.text.data())
        return _current;
    else
        throw std::logic_error("Cannot get token -- call next() and make sure it returns true.");
}

bool tokenizer::next()
{
    auto valid = [this] (const string_view& new_current, token_kind new_kind)
                 {
                     _current.text = new_current;
                     _current.kind = new_kind;
                     return true;
                 };
    
    if (!_current.text.empty())
        _position += _current.text.size();

    while (_position < _input.end())
    {
        token_kind kind;
        size_type  match_len;
        auto       result = detail::attempt_match(_position, _input.end(), *&kind, *&match_len);

        if (result == detail::match_result::complete_eof)
        {
            // Got full match
        }
        else if (result == detail::match_result::incomplete_eof)
        {
            // we couldn't read more data due to EOF...but we have an incomplete match
            kind = kind | token_kind::parse_error_indicator;
        }
        else if (result == detail::match_result::unmatched)
        {
            // unmatched entry -- this token is invalid
            kind = kind | token_kind::parse_error_indicator;
        }
        return valid(string_view(_position, match_len), kind);
    }

    return false;
}

void tokenizer::buffer_reserve(size_type)
{ }

}
