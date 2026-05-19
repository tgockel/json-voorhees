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
#include <jsonv/detail/architecture.hpp>

#include <cassert>
#include <cstddef>

#include "number.hpp"

#if JSONV_SSE2
#   include <emmintrin.h>
#endif

namespace jsonv::detail
{

// Chunked scan over a digit run: advance past consecutive '0'..'9' bytes
// and return the offset of the first non-digit. Zero means the byte at
// `iter` is already a non-digit; the state machine's switch handles it.
//
// SSE2-only: numbers in typical JSON are short enough (typically 10-15 byte
// runs) that an AVX2 32-byte path measured slower on canada.json -- the
// wider lanes don't help and the per-call overhead (256-bit broadcast,
// VZEROUPPER on exit) loses to the 16-byte path. JSONV_AVX2 stays available
// in architecture.hpp for callers where it pays off (see match_string).
#if JSONV_SSE2

__attribute__((noinline))
static std::size_t
fastforward_digits(const char* iter, const char* end) noexcept
{
    const char* const start = iter;
    const __m128i zero_v = _mm_set1_epi8('0');
    const __m128i nine_v = _mm_set1_epi8('9');

    while (iter + 16 <= end)
    {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(iter));
        // Signed compares: cmplt against '0' also catches high-bit (negative)
        // bytes, which are non-digits and correct to terminate on.
        __m128i lo     = _mm_cmplt_epi8(chunk, zero_v);
        __m128i hi     = _mm_cmpgt_epi8(chunk, nine_v);
        __m128i nondig = _mm_or_si128(lo, hi);
        unsigned mask  = static_cast<unsigned>(_mm_movemask_epi8(nondig)) & 0xFFFFu;

        if (mask == 0u)
        {
            iter += 16;
        }
        else
        {
            iter += __builtin_ctz(mask);
            return static_cast<std::size_t>(iter - start);
        }
    }
    return static_cast<std::size_t>(iter - start);
}

#else // !JSONV_SSE2

static inline std::size_t
fastforward_digits(const char*, const char*) noexcept
{
    return 0u;
}

#endif // JSONV_SSE2

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
        length += fastforward_digits(begin + length, end);
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
            length += fastforward_digits(begin + length, end);
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
        length += fastforward_digits(begin + length, end);
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
