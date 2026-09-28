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

    // Comments are off by default, but turning them on lets through more of what the mutator makes -- including
    // malformed bytes inside a comment, which must not stop a bad string after it being refused. Our own encoding
    // never writes a comment, so reading it back uses the defaults.
    jsonv::value original;
    try
    {
        original = jsonv::parse(text, jsonv::parse_options().comments(true));
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
