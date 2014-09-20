/** \file
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
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

const tokenizer::size_type tokenizer::min_buffer_size = 4096;

tokenizer::tokenizer(std::istream& input) :
        _input(input)
{
    buffer_reserve(min_buffer_size);
}

tokenizer::~tokenizer() noexcept = default;

const std::istream& tokenizer::input() const
{
    return _input;
}

std::pair<string_ref, token_kind> tokenizer::current() const
{
    if (_current.data())
        return { _current, _current_kind };
    else
        throw std::logic_error("Cannot get token -- call next() and make sure it returns true.");
}

static std::size_t position_in_buffer(const std::vector<char>& buffer, const string_ref& current)
{
    // an invalid current means the buffer is fresh, so we're at the start of it
    if (!current.data())
        return 0;
    
    std::ptrdiff_t pos = current.data() - buffer.data();
    assert(pos >= 0);
    assert(std::size_t(pos) < buffer.size());
    return std::size_t(pos);
}

bool tokenizer::next()
{
    auto valid = [this] (const string_ref& new_current, token_kind new_kind)
                 {
                     _current = new_current;
                     _current_kind = new_kind;
                     return true;
                 };
    auto invalid = [this]
                   {
                       _current.clear();
                       return false;
                   };
    
    size_type pos;
    bool nth_pass = false;
    while (true)
    {
        if (_buffer.empty() || (position_in_buffer(_buffer, _current) + _current.size()) == _buffer.size())
        {
            if (!read_input(nth_pass))
                return invalid();
            nth_pass = true;
        }
        
        pos = position_in_buffer(_buffer, _current) + _current.size();
        
        token_kind kind;
        size_type match_len;
        auto result = detail::attempt_match(_buffer.data() + pos, _buffer.data() + _buffer.size(),
                                            kind, match_len
                                           );
        if (result == detail::match_result::complete_eof || result == detail::match_result::incomplete_eof)
        {
            // partial match...we need to grow the buffer to see if it keeps going
            if (read_input(true))
                continue;
            else if (result == detail::match_result::incomplete_eof)
                // we couldn't read more data due to EOF...but we have an incomplete match
                kind = kind | token_kind::parse_error_indicator;
        }
        return valid(string_ref(_buffer.data() + pos, match_len), kind);
    }
}

bool tokenizer::read_input(bool grow_buffer)
{
    auto old_buffer_pos = position_in_buffer(_buffer, _current);
    
    char* buffer_write_pos;
    size_type buffer_write_size;
    if (grow_buffer)
    {
        buffer_write_size = min_buffer_size;
        auto offset = _buffer.size();
        _buffer.resize(_buffer.size() + buffer_write_size);
        buffer_write_pos = _buffer.data() + offset;
    }
    else
    {
        buffer_write_size = _buffer.capacity();
        _buffer.resize(buffer_write_size);
        buffer_write_pos = _buffer.data();
    }
    
    _input.read(buffer_write_pos, buffer_write_size);
    auto read_count = _input.gcount();
    if (read_count > 0)
    {
        _buffer.resize(_input.gcount());
        _current = string_ref(_buffer.data() + old_buffer_pos, _current.size());
        return true;
    }
    else
    {
        return false;
    }
}

void tokenizer::buffer_reserve(size_type sz)
{
    _buffer.reserve(std::max(sz, min_buffer_size));
}

}
