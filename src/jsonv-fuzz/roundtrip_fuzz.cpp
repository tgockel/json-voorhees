/// \file
/// libFuzzer target checking that encoding and parsing agree with each other.
///
/// Unlike the `parse` target, this one only runs when the input parses successfully, and it checks invariants rather
/// than merely "did not crash". Everything after the initial parse operates on *our own* output, so any exception or
/// mismatch there is a finding.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/parse.hpp>
#include <jsonv/value.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>

namespace
{

/// Does \a text contain a UTF-8 sequence that the parser accepts but that is not the canonical encoding of its
/// codepoint?
///
/// This is exactly the set of inputs described by issue #207: overlong encodings, UTF-16 surrogates written as raw
/// bytes, codepoints above U+10FFFF, and the 5- and 6-byte forms removed from UTF-8 by RFC 3629. `match_string` lets
/// all of them through, but the encoder decodes each sequence to a codepoint and re-emits it canonically, so the value
/// legitimately changes across a round trip -- `["\xc0\x80"]` encodes to `["\u0000"]`, two bytes becoming one.
///
/// \note
/// Delete this along with the round-trip caller once #207 is fixed and the parser rejects these inputs outright.
bool has_non_canonical_utf8(std::string_view text) noexcept
{
    auto is_continuation = [](unsigned char c) noexcept { return (c & 0xc0u) == 0x80u; };

    for (std::size_t idx = 0; idx < text.size(); )
    {
        const auto lead = static_cast<unsigned char>(text[idx]);

        if (lead < 0x80u)
        {
            ++idx;
            continue;
        }

        std::size_t   length    = 0;
        std::uint32_t codepoint = 0;
        if ((lead & 0xe0u) == 0xc0u)      { length = 2; codepoint = lead & 0x1fu; }
        else if ((lead & 0xf0u) == 0xe0u) { length = 3; codepoint = lead & 0x0fu; }
        else if ((lead & 0xf8u) == 0xf0u) { length = 4; codepoint = lead & 0x07u; }
        else
        {
            // A 5- or 6-byte lead, or a stray continuation byte. `match_string` accepts the former.
            return true;
        }

        if (idx + length > text.size())
            return false;   // Truncated; the parser rejects it, so it never reaches the oracle.

        for (std::size_t offset = 1; offset < length; ++offset)
        {
            const auto next = static_cast<unsigned char>(text[idx + offset]);
            if (!is_continuation(next))
                return false;   // Malformed; rejected by the parser.
            codepoint = (codepoint << 6) | (next & 0x3fu);
        }

        static constexpr std::uint32_t minimum[] = { 0, 0, 0x80, 0x800, 0x10000 };
        if (codepoint < minimum[length])
            return true;                                        // Overlong.
        if (codepoint >= 0xd800 && codepoint <= 0xdfff)
            return true;                                        // Surrogate.
        if (codepoint > 0x10ffff)
            return true;                                        // Above the Unicode ceiling.

        idx += length;
    }

    return false;
}

/// Report a violated invariant and abort.
///
/// Deliberately not `assert`, which `NDEBUG` would compile away in the RelWithDebInfo build the fuzzers use, silently
/// turning the oracle into a no-op. libFuzzer saves the input and prints it base64-encoded, but a readable hex dump of
/// the exact bytes is what you actually want to look at first.
[[noreturn]]
void fail(const char* what, std::string_view input)
{
    std::fprintf(stderr, "\nround-trip invariant violated: %s\ninput (%zu bytes): ", what, input.size());
    for (unsigned char c : input)
    {
        if (c >= 0x20 && c < 0x7f && c != '\\')
            std::fputc(c, stderr);
        else
            std::fprintf(stderr, "\\x%02x", c);
    }
    std::fputc('\n', stderr);
    std::fflush(stderr);
    std::abort();
}

void check(bool condition, const char* what, std::string_view input)
{
    if (!condition)
        fail(what, input);
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    // libFuzzer copies each input into an allocation of exactly `size` bytes before invoking this function, so an
    // AddressSanitizer redzone already sits immediately after the final byte and a read past the end is reported.
    //
    // `parse_index` stores raw pointers into this text rather than copying it, so the buffer has to outlive every
    // index built from it and every `extract_tree` call made against one. libFuzzer frees it after we return, which
    // is why nothing here may outlive the call.
    const std::string_view text(reinterpret_cast<const char*>(data), size);

    jsonv::value original;
    try
    {
        original = jsonv::parse(text);
    }
    catch (const std::exception&)
    {
        // Invalid input is the `parse` target's job, not this one's.
        return 0;
    }

    // From here down, a throw means we produced JSON we cannot read back.
    try
    {
        const std::string encoded = jsonv::to_string(original);

        // Input carrying UTF-8 that the parser accepts but that is not canonical is held out of the oracle. The
        // encoder decodes every sequence and re-emits it in canonical form, which changes the value outright
        // (`["\xc0\x80"]` becomes `["\u0000"]`) and, for a raw surrogate, yields output the parser
        // then rejects (`["\xed\xa0\x80"]` becomes `["\ud800"]`). Both are issue #207 -- the parser
        // should never have accepted the input -- rather than defects in the round trip itself.
        //
        // Remove this, and `has_non_canonical_utf8` with it, once #207 lands.
        if (has_non_canonical_utf8(text))
            return 0;

        const jsonv::value reparsed  = jsonv::parse(encoded);
        const std::string  reencoded = jsonv::to_string(reparsed);

        // Encoding must be a fixed point: whatever normalization the first encode performs has to have settled by
        // the second one.
        check(encoded == reencoded, "encoding did not reach a fixed point", text);

        // Re-parsing our own encoding must produce an equal value. A decimal whose shortest representation has no
        // fractional part or exponent (`2.0` prints as `2`) comes back as an integer, which is fine --
        // `compare_traits` ranks integer and decimal together and compares them numerically.
        check(original == reparsed, "re-parsing our own encoding produced a different value", text);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "\nre-parsing our own output failed: %s\n", ex.what());
        std::fflush(stderr);
        throw;
    }

    return 0;
}
