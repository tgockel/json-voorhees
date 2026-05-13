/// \file
/// Pattern matching for JSON numeric values.
///
/// Copyright (c) 2014-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/config.hpp>

#include <cassert>

#include "number.hpp"

namespace jsonv::detail
{

namespace
{

enum class match_number_state
{
    initial,
    leading_minus,
    leading_zero,
    integer,
    decimal,
    exponent,
    exponent_sign,
    complete,
};

}

match_number_result match_number(const char* begin, const char* end)
{
    auto length     = std::size_t(0);
    auto state      = match_number_state::initial;
    auto max_length = std::size_t(end - begin);
    auto decimal    = false;

    auto current = [&] ()
                   {
                       if (length < max_length)
                           return begin[length];
                       else
                           return '\0';
                   };

    // Initial: behavior of parse branches from here
    switch (current())
    {
    case '-':
        ++length;
        state = match_number_state::leading_minus;
        break;
    case '0':
        ++length;
        state = match_number_state::leading_zero;
        break;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        ++length;
        state = match_number_state::integer;
        break;
    default:
        return match_number_result::create_unmatched(length);
    }

    // Leading '-'
    if (state == match_number_state::leading_minus)
    {
        switch (current())
        {
        case '0':
            ++length;
            state = match_number_state::leading_zero;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            ++length;
            state = match_number_state::integer;
            break;
        default:
            return match_number_result::create_unmatched(length);
        }
    }

    // Leading '0' or "-0"
    if (state == match_number_state::leading_zero)
    {
        switch (current())
        {
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
            return match_number_result::create_unmatched(length);
        case '.':
            ++length;
            state   = match_number_state::decimal;
            decimal = true;
            break;
        case 'e':
        case 'E':
            ++length;
            state   = match_number_state::exponent;
            decimal = true;
            break;
        default:
            state = match_number_state::complete;
            break;
        }
    }

    // Have only seen integer values
    while (state == match_number_state::integer)
    {
        switch (current())
        {
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
            ++length;
            break;
        case '.':
            ++length;
            state   = match_number_state::decimal;
            decimal = true;
            break;
        case 'e':
        case 'E':
            ++length;
            state   = match_number_state::exponent;
            decimal = true;
            break;
        default:
            state = match_number_state::complete;
            break;
        }
    }

    // Just saw a '.'
    if (state == match_number_state::decimal)
    {
        switch (current())
        {
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
            ++length;
            break;
        default:
            return match_number_result::create_unmatched(length);
        }

        while (state == match_number_state::decimal)
        {
            switch (current())
            {
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
                ++length;
                break;
            case 'e':
            case 'E':
                ++length;
                state   = match_number_state::exponent;
                decimal = true;
                break;
            default:
                state = match_number_state::complete;
                break;
            }
        }
    }

    // Just saw 'e' or 'E'
    if (state == match_number_state::exponent)
    {
        switch (current())
        {
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
            ++length;
            break;
        case '+':
        case '-':
            ++length;
            state = match_number_state::exponent_sign;
            break;
        default:
            return match_number_result::create_unmatched(length);
        }
    }

    // Just saw "e-", "e+", "E-", or "E+"
    if (state == match_number_state::exponent_sign)
    {
        switch (current())
        {
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
            ++length;
            state = match_number_state::exponent;
            break;
        default:
            return match_number_result::create_unmatched(length);
        }
    }

    while (state == match_number_state::exponent)
    {
        switch (current())
        {
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
            ++length;
            break;
        default:
            state = match_number_state::complete;
            break;
        }
    }

    assert(state == match_number_state::complete);
    return match_number_result::create_complete(decimal, length);
}

}
