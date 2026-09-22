/** \file
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "detail.hpp"
#include "char_convert.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <system_error>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// kind                                                                                                               //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* kind_desc(kind type)
{
    switch (type)
    {
    case kind::object:
        return "object";
    case kind::array:
        return "array";
    case kind::string:
        return "string";
    case kind::integer:
        return "integer";
    case kind::decimal:
        return "decimal";
    case kind::boolean:
        return "boolean";
    case kind::null:
        return "null";
    default:
        return "UNKNOWN";// should never happen
    }
}

bool kind_valid(kind k)
{
    switch (k)
    {
    case kind::object:
    case kind::array:
    case kind::string:
    case kind::integer:
    case kind::decimal:
    case kind::boolean:
    case kind::null:
        return true;
    default:
        return false;
    }
}

void check_type(kind expected, kind actual)
{
    if (expected != actual)
    {
        std::ostringstream stream;
        stream << "Unexpected type: expected " << kind_desc(expected)
               << " but found " << kind_desc(actual) << ".";
        throw kind_error(stream.str());
    }
}

void check_type(std::initializer_list<kind> expected, kind actual)
{
    if (std::none_of(expected.begin(), expected.end(), [actual] (kind x) { return x == actual; }))
    {
        std::ostringstream stream;
        stream << "Unexpected type: expected ";
        std::size_t num = 1;
        for (kind k : expected)
        {
            stream << kind_desc(k);
            if (num + 1 < expected.size())
                stream << ", ";
            else if (num < expected.size())
                stream << " or ";
            ++num;
        }
        stream << " but found " << kind_desc(actual) << ".";
        throw kind_error(stream.str());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Printing                                                                                                           //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::ostream& stream_escaped_string(std::ostream& stream, std::string_view str, bool ensure_ascii)
{
    stream << "\"";
    detail::string_encode(stream, str, ensure_ascii);
    stream << "\"";
    return stream;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Numbers                                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::optional<std::string_view> format_decimal(double value, char* buffer)
{
    // Two bytes short of the end, so the `.0` fixup below can never run off it. `general` needs at most 24 for a
    // `double`, so nothing is actually given up.
    auto result = std::to_chars(buffer, buffer + number_token_max - 2U, value, std::chars_format::general);
    if (result.ec != std::errc{})
        return std::nullopt;

    std::string_view text(buffer, static_cast<std::size_t>(result.ptr - buffer));

    // `general` gives the shortest representation that round-trips, which for an integral value carries neither a
    // decimal point nor an exponent -- `2.0` prints as `2` and `-0.0` as `-0`. Those re-parse as `kind::integer`
    // rather than `kind::decimal`, and negative zero loses its sign along the way, which also makes encoding unstable
    // across a parse/encode cycle. Append a fractional part so the token stays a decimal. See issue #208.
    if (text.find_first_of(".eE") == std::string_view::npos)
    {
        buffer[text.size()]      = '.';
        buffer[text.size() + 1U] = '0';
        text = std::string_view(buffer, text.size() + 2U);
    }

    return text;
}

std::optional<std::string_view> format_integer(std::int64_t value, char* buffer)
{
    auto result = std::to_chars(buffer, buffer + number_token_max, value);
    if (result.ec != std::errc{})
        return std::nullopt;

    return std::string_view(buffer, static_cast<std::size_t>(result.ptr - buffer));
}

}
