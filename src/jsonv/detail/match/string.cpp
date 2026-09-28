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
#include <jsonv/detail/architecture.hpp>
#include <jsonv/detail/utf8.hpp>

#include <cassert>
#include <cctype>
#include <cstdint>

#include "string.hpp"

#include <iostream>

#if JSONV_SSE2
#   include <emmintrin.h>
#endif
#if JSONV_AVX2
#   include <immintrin.h>
#endif

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

// Chunked scan over the string body: advance past runs of "boring" bytes and
// stop at the first byte that needs scalar handling -- closing quote, backslash,
// UTF-8 lead byte, or (in strict mode) an unprintable ASCII control / DEL byte.
// Returns the number of bytes safely skipped from `iter`. Zero means the byte
// at `iter` is already interesting; the caller's scalar loop handles it.
#if JSONV_SSE2

template <bool CheckPrintability>
__attribute__((noinline))
static std::size_t
ffsb_sse2(const char* iter, const char* end) noexcept
{
    const char* const start = iter;
    const __m128i quote_v      = _mm_set1_epi8('\"');
    const __m128i bslash_v     = _mm_set1_epi8('\\');
    const __m128i ctrl_limit_v = _mm_set1_epi8(0x20);
    const __m128i del_v        = _mm_set1_epi8(0x7f);

    while (iter + 16 <= end)
    {
        __m128i chunk    = _mm_loadu_si128(reinterpret_cast<const __m128i*>(iter));
        __m128i m_quote  = _mm_cmpeq_epi8(chunk, quote_v);
        __m128i m_bslash = _mm_cmpeq_epi8(chunk, bslash_v);

        unsigned mask;
        if constexpr (CheckPrintability)
        {
            // Signed cmplt also flags high-bit bytes (negative) -- one op covers
            // both the < 0x20 strict rejection and the UTF-8 lead-byte case.
            __m128i m_ctrl = _mm_cmpgt_epi8(ctrl_limit_v, chunk);
            __m128i m_del  = _mm_cmpeq_epi8(chunk, del_v);
            __m128i hit    = _mm_or_si128(
                _mm_or_si128(m_quote, m_bslash),
                _mm_or_si128(m_ctrl,  m_del)
            );
            mask = static_cast<unsigned>(_mm_movemask_epi8(hit)) & 0xFFFFu;
        }
        else
        {
            __m128i hit = _mm_or_si128(m_quote, m_bslash);
            mask = (static_cast<unsigned>(_mm_movemask_epi8(hit))
                  | static_cast<unsigned>(_mm_movemask_epi8(chunk))) & 0xFFFFu;
        }

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

#if JSONV_AVX2

template <bool CheckPrintability>
__attribute__((target("avx2")))
static std::size_t
ffsb_avx2(const char* iter, const char* end) noexcept
{
    const char* const start = iter;
    const __m256i quote_v      = _mm256_set1_epi8('\"');
    const __m256i bslash_v     = _mm256_set1_epi8('\\');
    const __m256i ctrl_limit_v = _mm256_set1_epi8(0x20);
    const __m256i del_v        = _mm256_set1_epi8(0x7f);

    while (iter + 32 <= end)
    {
        __m256i chunk    = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(iter));
        __m256i m_quote  = _mm256_cmpeq_epi8(chunk, quote_v);
        __m256i m_bslash = _mm256_cmpeq_epi8(chunk, bslash_v);

        std::uint32_t mask;
        if constexpr (CheckPrintability)
        {
            __m256i m_ctrl = _mm256_cmpgt_epi8(ctrl_limit_v, chunk);
            __m256i m_del  = _mm256_cmpeq_epi8(chunk, del_v);
            __m256i hit    = _mm256_or_si256(
                _mm256_or_si256(m_quote, m_bslash),
                _mm256_or_si256(m_ctrl,  m_del)
            );
            mask = static_cast<std::uint32_t>(_mm256_movemask_epi8(hit));
        }
        else
        {
            __m256i hit = _mm256_or_si256(m_quote, m_bslash);
            mask = static_cast<std::uint32_t>(_mm256_movemask_epi8(hit))
                 | static_cast<std::uint32_t>(_mm256_movemask_epi8(chunk));
        }

        if (mask == 0u)
        {
            iter += 32;
        }
        else
        {
            iter += __builtin_ctz(mask);
            return static_cast<std::size_t>(iter - start);
        }
    }
    return static_cast<std::size_t>(iter - start);
}

#endif // JSONV_AVX2

template <bool CheckPrintability>
JSONV_ALWAYS_INLINE static inline std::size_t
fastforward_string_body(const char* iter, const char* end) noexcept
{
#if JSONV_AVX2
    static const bool has_avx2 = __builtin_cpu_supports("avx2");
    if (has_avx2)
        return ffsb_avx2<CheckPrintability>(iter, end);
    else
        return ffsb_sse2<CheckPrintability>(iter, end);
#else
    return ffsb_sse2<CheckPrintability>(iter, end);
#endif
}

#else // !JSONV_SSE2

template <bool>
JSONV_ALWAYS_INLINE static inline std::size_t
fastforward_string_body(const char*, const char*) noexcept
{
    return 0u;
}

#endif // JSONV_SSE2

match_string_result match_string(const char* iter, const char* end, const parse_options& options)
{
    assert(*iter == '\"');

    ++iter;
    std::size_t length  = 1U;
    bool        escaped = false;
    const bool  check_printability = options.string_encoding() == parse_options::encoding::utf8_strict;

    while (iter < end)
    {
        const std::size_t skip = check_printability
                               ? fastforward_string_body<true>(iter, end)
                               : fastforward_string_body<false>(iter, end);
        iter   += skip;
        length += skip;
        if (iter >= end)
            break;

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

                // Through `unsigned char`: `char` is signed here, so a byte above 0x7f arrives as a negative
                // value, and the `<cctype>` classifiers are defined only for `unsigned char` values and `EOF`.
                // Anything else is undefined, and the MSVC Debug runtime says so out loud -- `isctype.cpp`
                // asserts `c >= -1 && c <= 255`. A high byte in a `\u` escape is malformed input the parser
                // should simply reject, not a diagnostic it should trip over.
                if (  std::isxdigit(static_cast<unsigned char>(iter[0]))
                   && std::isxdigit(static_cast<unsigned char>(iter[1]))
                   && std::isxdigit(static_cast<unsigned char>(iter[2]))
                   && std::isxdigit(static_cast<unsigned char>(iter[3]))
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
            JSONV_LIKELY
            if (check_printability && !is_print(*iter))
            {
                JSONV_UNLIKELY
                return match_string_result::create_unmatched(length);
            }

            ++iter;
            ++length;
        }
        else if (auto utf_seq_length = utf8_sequence_length(iter, end))
        {
            iter   += utf_seq_length;
            length += utf_seq_length;
        }
        else
        {
            // Not well-formed UTF-8: a stray continuation byte, a truncated sequence, or one which encodes an overlong
            // form, a surrogate or a codepoint above U+10FFFF.
            return match_string_result::create_unmatched(length);
        }
    }

    return match_string_result::create_unmatched(length);
}

}
