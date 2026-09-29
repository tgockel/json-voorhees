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
#include <jsonv/coerce.hpp>
#include <jsonv/algorithm.hpp>
#include <jsonv/value.hpp>

#include <charconv>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>
#include <system_error>

#include "detail/fallthrough.hpp"
#include "detail/fast_float/fast_float.h"
#include "detail/match/number.hpp"

namespace jsonv
{

namespace
{

// A number spelt the way JSON spells one, with the whitespace that may surround it already trimmed off.
struct number_token
{
    std::string_view text;
    bool             decimal;
};

// The number `source` holds, if all it holds is one JSON number and the whitespace JSON allows around a value. This is
// the parser's own grammar for a number, but applied here rather than by `parse`, so nothing `parse_options` can switch
// on -- comments, most obviously -- is allowed in. It is also what keeps out the spellings the conversions below would
// take and JSON does not: `fast_float` reads `inf` and `nan`, and `from_chars` reads a leading zero.
std::optional<number_token> match_number_token(std::string_view source)
{
    constexpr std::string_view whitespace = " \t\n\r";

    auto first = source.find_first_not_of(whitespace);
    if (first == std::string_view::npos)
        return std::nullopt;

    auto last = source.find_last_not_of(whitespace);
    auto text = source.substr(first, last - first + 1);

    auto matched = detail::match_number(text.data(), text.data() + text.size());
    if (matched && matched.length == text.size())
        return number_token{ text, matched.decimal };
    else
        return std::nullopt;
}

// The nearest `double` to `token`, which has already matched the JSON number grammar. A magnitude which underflows to
// zero is zero, as it is to `parse`; one with no finite `double` at all -- `1e400` -- is not a number this can give.
std::optional<double> decimal_from_token(std::string_view token)
{
    auto   end = token.data() + token.size();
    double out{};
    auto   result = fast_float::from_chars(token.data(), end, out, fast_float::chars_format::general);
    if (  result.ptr == end
       && (  result.ec == std::errc{}
          || (result.ec == std::errc::result_out_of_range && out == 0.0)
          )
       )
        return out;
    else
        return std::nullopt;
}

std::int64_t integer_from_decimal(double src)
{
    // Converting a double that does not fit the destination type is undefined behavior rather
    // than a saturating conversion, so the value has to be range-checked first. The upper test
    // is `>=` because `double(int64_t max)` rounds up to 2^63, one past the largest
    // representable int64_t; the lower bound is -2^63 exactly, so `<=` there is only belt and
    // braces. NaN compares false against everything and has no meaningful clamp, so it gets 0.
    if (std::isnan(src))
        return 0;
    else if (src >= double(std::numeric_limits<std::int64_t>::max()))
        return std::numeric_limits<std::int64_t>::max();
    else if (src <= double(std::numeric_limits<std::int64_t>::min()))
        return std::numeric_limits<std::int64_t>::min();
    else
        return std::int64_t(src);
}

// The integer `source` holds, if it holds a JSON number. One which fits in an `std::int64_t` is read exactly; any other
// -- a decimal, or an integer too large for 64 bits -- goes by way of its nearest `double`, which is truncated and
// clamped just as a `kind::decimal` is.
std::optional<std::int64_t> integer_from_string(std::string_view source)
{
    auto token = match_number_token(source);
    if (!token)
        return std::nullopt;

    if (!token->decimal)
    {
        auto         end = token->text.data() + token->text.size();
        std::int64_t out{};
        auto         result = std::from_chars(token->text.data(), end, out);
        if (result.ec == std::errc{} && result.ptr == end)
            return out;
    }

    if (auto approx = decimal_from_token(token->text))
        return integer_from_decimal(*approx);
    else
        return std::nullopt;
}

std::optional<double> decimal_from_string(std::string_view source)
{
    if (auto token = match_number_token(source))
        return decimal_from_token(token->text);
    else
        return std::nullopt;
}

}

bool can_coerce(const kind& from, const kind& to)
{
    switch (to)
    {
    case kind::null:
    case kind::object:
    case kind::array:
        // object, array and null cannot be coerced to, so the kinds must match
        return from == to;
    case kind::string:
    case kind::boolean:
        return true;
    case kind::decimal:
    case kind::integer:
        return from == kind::decimal || from == kind::integer;
    default:
        // can't coerce to a corrupt kind
        return false;
    }
}

bool can_coerce(const value& from, const kind& to)
{
    if (can_coerce(from.kind(), to))
    {
        return true;
    }
    else if (from.kind() == kind::string && (to == kind::decimal || to == kind::integer))
    {
        // Actually attempt the conversion from string into the proper number. If it succeeds, we can coerce the string.
        try
        {
            // Called for the throw -- the converted number itself is not interesting here.
            if (to == kind::decimal)
                (void) coerce_decimal(from);
            else
                (void) coerce_integer(from);
            return true;
        }
        catch (const kind_error&)
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

std::nullptr_t coerce_null(const value& from)
{
    if (from.kind() == kind::null)
        return nullptr;
    else
        throw kind_error(std::string("Can only coerce null from a null, but from is of kind ")
                         + to_string(from.kind())
                        );
}

std::map<std::string, value> coerce_object(const value& from)
{
    if (from.kind() == kind::object)
        return std::map<std::string, value>(from.begin_object(), from.end_object());
    else
        throw kind_error(std::string("Invalid kind for object: ") + to_string(from.kind()));
}

std::vector<value> coerce_array(const value& from)
{
    if (from.kind() == kind::array)
        return std::vector<value>(from.begin_array(), from.end_array());
    else
        throw kind_error(std::string("Invalid kind for array: ") + to_string(from.kind()));
}

std::string coerce_string(const value& from)
{
    if (from.kind() == kind::string)
        return from.as_string();
    else
        return to_string(from);
}

std::int64_t coerce_integer(const value& from)
{
    switch (from.kind())
    {
    case kind::boolean:
        return from.as_boolean() ? 1 : 0;
    case kind::integer:
        return from.as_integer();
    case kind::decimal:
        return integer_from_decimal(from.as_decimal());
    case kind::string:
        if (auto out = integer_from_string(from.as_string_view()))
            return *out;
        else
            throw kind_error(std::string("Could not interpret string ") + to_string(from) + " as an integer.");
    case kind::null:
    case kind::object:
    case kind::array:
    default:
        throw kind_error(std::string("Invalid kind for integer: ") + to_string(from.kind()));
    }
}

double coerce_decimal(const value& from)
{
    switch (from.kind())
    {
    case kind::boolean:
        return from.as_boolean() ? 1.0 : 0.0;
    case kind::integer:
    case kind::decimal:
        return from.as_decimal();
    case kind::string:
        if (auto out = decimal_from_string(from.as_string_view()))
            return *out;
        else
            throw kind_error(std::string("Could not interpret string ") + to_string(from) + " as a decimal.");
    case kind::null:
    case kind::object:
    case kind::array:
    default:
        throw kind_error(std::string("Invalid kind for decimal: ") + to_string(from.kind()));
    }
}

bool coerce_boolean(const value& from)
{
    switch (from.kind())
    {
    case kind::null:
        return false;
    case kind::object:
    case kind::array:
    case kind::string:
        return !from.empty();
    case kind::integer:
        return from != 0;
    case kind::decimal:
        return from != 0.0;
    case kind::boolean:
        return from.as_boolean();
    default:
        throw kind_error(std::string("Invalid kind for boolean: ") + to_string(from.kind()));
    }
}

value coerce_merge(value a, value b)
{
    if (a.kind() == b.kind())
        return merge_recursive(std::move(a), std::move(b));
    else if (b.kind() == kind::null)
        return a;
    else switch (a.kind())
    {
    case kind::array:
        a.push_back(std::move(b));
        return a;
    case kind::boolean:
        return a.as_boolean() || coerce_boolean(b);
    case kind::integer:
        if (can_coerce(b, kind::integer))
            return a.as_integer() + coerce_integer(b);
        JSONV_FALLTHROUGH();
    case kind::decimal:
        if (can_coerce(b, kind::decimal))
            return a.as_decimal() + coerce_decimal(b);
        else
            return coerce_merge(std::move(b), std::move(a));
    case kind::null:
        return b;
    case kind::object:
        a["undefined"] = std::move(b);
        return a;
    case kind::string:
        return a.as_string() + coerce_string(b);
    default:
        throw kind_error(to_string(a.kind()));
    }
}

}
