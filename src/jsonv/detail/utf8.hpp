/// \file
/// What counts as well-formed UTF-8.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <cstddef>

namespace jsonv::detail
{

/// The length of the well-formed UTF-8 sequence starting at \a iter, or 0 if the bytes before \a end do not begin one.
/// \a iter must be before \a end.
///
/// Well-formed is Unicode's definition (Table 3-7 in chapter 3 of the standard, which RFC 3629 matches): a sequence of
/// at most 4 bytes, which is the shortest encoding of its codepoint, and whose codepoint is neither a UTF-16 surrogate
/// (U+D800 through U+DFFF) nor above U+10FFFF. Each of those rules is a limit on the lead byte or on the byte after it,
/// so nothing here decodes. The parser, the string decoder and the string encoder all ask this function, so none of
/// them can accept a sequence the others would refuse.
JSONV_ALWAYS_INLINE inline constexpr unsigned utf8_sequence_length(const char* iter, const char* end) noexcept
{
    // NOTE(tgockel): `char` is signed here, so every comparison is made on an `unsigned char`.
    const auto lead = static_cast<unsigned char>(iter[0]);

    unsigned      length     = 0U;
    unsigned char second_min = 0x80U;
    unsigned char second_max = 0xbfU;
    if (lead < 0x80U)
    {
        return 1U;
    }
    else if (lead < 0xc2U)
    {
        // A continuation byte, or 0xC0 or 0xC1, which only ever begin an overlong encoding of an ASCII codepoint.
        return 0U;
    }
    else if (lead < 0xe0U)
    {
        length = 2U;
    }
    else if (lead < 0xf0U)
    {
        length = 3U;
        if (lead == 0xe0U)
            second_min = 0xa0U; // Anything lower is an overlong encoding.
        else if (lead == 0xedU)
            second_max = 0x9fU; // Anything higher is a surrogate.
    }
    else if (lead < 0xf5U)
    {
        length = 4U;
        if (lead == 0xf0U)
            second_min = 0x90U; // Anything lower is an overlong encoding.
        else if (lead == 0xf4U)
            second_max = 0x8fU; // Anything higher is above U+10FFFF.
    }
    else
    {
        // 0xF5 through 0xF7 only ever begin a codepoint above U+10FFFF, 0xF8 through 0xFD begin the 5- and 6-byte forms
        // RFC 3629 removed, and 0xFE and 0xFF were never part of UTF-8.
        return 0U;
    }

    if (end - iter < static_cast<std::ptrdiff_t>(length))
        return 0U;

    const auto second = static_cast<unsigned char>(iter[1]);
    if (second < second_min || second > second_max)
        return 0U;

    for (unsigned offset = 2U; offset < length; ++offset)
    {
        if ((static_cast<unsigned char>(iter[offset]) & 0xc0U) != 0x80U)
            return 0U;
    }

    return length;
}

}
