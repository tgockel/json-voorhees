/// \file
/// Pattern matching for JSON string values.
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/config.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/detail/is_print.hpp>

#include <cassert>
#include <cctype>

#include "string.hpp"

#include <iostream>

namespace jsonv::detail
{

static bool is_valid_escape(char c)
{
    switch (c)
    {
    case 'b':
    case 'f':
    case 'n':
    case 'r':
    case 't':
    case '\\':
    case '/':
    case '\"':
        return true;
    case 'u': // <- note that `u` must be handled in a special case
    default:
        return false;
    }
}

static constexpr bool char_bitmatch(char c, char pos, char neg)
{
    using u8 = unsigned char;

    // NOTE(tgockel, 2018-06-04): The use of casting should not be needed here. However, GCC 8.1.0 seems to have a
    // bug in the optimizer that causes this function to erroneously return `true` in some cases (specifically, with
    // `char_bitmatch('\xf0', '\xc0', '\x20')`, but not if you call the function directly). It is possible this issue
    // (https://github.com/tgockel/json-voorhees/issues/108) has been misdiagnosed, but the behavior only happens on
    // GCC 8.1.0 and only with -O3. This also fixes the problem, even though it logically should not change anything
    // (https://stackoverflow.com/questions/50671485/bitwise-operations-on-signed-chars).
    return (u8(c) & u8(pos)) == u8(pos)
        && !(u8(c) & u8(neg));
}

/// Tests if \a c is a valid UTF-8 sequence continuation.
static constexpr bool is_utf8_sequence_continuation(char c)
{
    return char_bitmatch(c, '\x80', '\x40');
}

static unsigned utf8_length(char c)
{
    if (char_bitmatch(c, '\xc0', '\x20'))
    {
        JSONV_LIKELY
        return 2U;
    }
    else if (char_bitmatch(c, '\xe0', '\x10'))
    {
        return 3U;
    }
    else if (char_bitmatch(c, '\xf0', '\x08'))
    {
        return 4U;
    }
    else if (char_bitmatch(c, '\xf8', '\x04'))
    {
        return 5U;
    }
    else if (char_bitmatch(c, '\xfc', '\x02'))
    {
        return 6U;
    }
    else
    {
        JSONV_UNLIKELY
        // This is not an acceptable/valid UTF-8 string. A failure here means I can't trust or don't understand the
        // source encoding.
        return 0U;
    }
}

match_string_result match_string(const char* iter, const char* end, const parse_options& options)
{
    assert(*iter == '\"');

    ++iter;
    std::size_t length  = 1U;
    bool        escaped = false;
    const bool  check_printability = options.string_encoding() == parse_options::encoding::utf8_strict;

    while (iter < end)
    {
        if (*iter == '\"')
        {
            ++length;
            return match_string_result::create_complete(escaped, length);
        }
        else if (*iter == '\\')
        {
            escaped = true;
            if (iter + 1 == end)
            {
                JSONV_UNLIKELY
                return match_string_result::create_unmatched(length);
            }
            else if (iter[1] == 'u')
            {
                iter   += 2;
                length += 2;

                if (iter + 4 >= end)
                {
                    JSONV_UNLIKELY
                    return match_string_result::create_unmatched(length);
                }

                if (  std::isxdigit(iter[0])
                   && std::isxdigit(iter[1])
                   && std::isxdigit(iter[2])
                   && std::isxdigit(iter[3])
                   )
                {
                    JSONV_LIKELY
                    iter   += 4;
                    length += 4;
                }
                else
                {
                    return match_string_result::create_unmatched(length);
                }
            }
            else if (is_valid_escape(iter[1]))
            {
                JSONV_LIKELY
                length += 2;
                iter   += 2;
            }
            else
            {
                return match_string_result::create_unmatched(length);
            }
        }
        else if (!(*iter & '\x80'))
        {
            if (check_printability && !is_print(*iter))
            {
                JSONV_UNLIKELY
                return match_string_result::create_unmatched(length);
            }

            ++iter;
            ++length;
        }
        else if (auto utf_seq_length = utf8_length(*iter))
        {
            if (iter + utf_seq_length > end)
            {
                return match_string_result::create_unmatched(length);
            }
            else
            {
                for (unsigned offset = 1U; offset < utf_seq_length; ++offset)
                {
                    if (!is_utf8_sequence_continuation(iter[offset]))
                    {
                        JSONV_UNLIKELY
                        return match_string_result::create_unmatched(length + offset);
                    }
                }

                iter   += utf_seq_length;
                length += utf_seq_length;
            }
        }
        else
        {
            return match_string_result::create_unmatched(length);
        }
    }

    return match_string_result::create_unmatched(length);
}

}
