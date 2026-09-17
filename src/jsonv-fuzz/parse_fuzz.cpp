/// \file
/// libFuzzer target exercising the parser for crashes, hangs, and sanitizer findings.
///
/// The bug bar here is crashes, hangs, UB, and leaks -- *not* rejection of invalid input. `jsonv::parse_error` and the
/// other documented exceptions are normal outcomes for malformed JSON and are swallowed.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/parse.hpp>
#include <jsonv/parse_index.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    // libFuzzer copies each input into an allocation of exactly `size` bytes before calling us, so an
    // AddressSanitizer redzone already sits immediately after the final byte and a read past the end is reported.
    //
    // `parse_index` stores raw pointers into this text rather than copying it, so nothing built from it may outlive
    // this call.
    const std::string_view text(reinterpret_cast<const char*>(data), size);

    // Each of these reaches branches the others do not: `create_strict` switches on utf8_strict *and* drops the depth
    // limit to 20, requires a document at the top level, and disallows comments; the third isolates the utf8_strict
    // printability check in `match_string` from those other three changes; the fourth isolates comments, which are
    // accepted by default.
    const jsonv::parse_options options[] =
        {
            jsonv::parse_options::create_default(),
            jsonv::parse_options::create_strict(),
            jsonv::parse_options().string_encoding(jsonv::parse_options::encoding::utf8_strict),
            jsonv::parse_options().comments(false),
        };

    for (const auto& opts : options)
    {
        // `jsonv::parse` is `parse_index::parse` + `validate()` + `extract_tree()`, so this covers both stages.
        try
        {
            (void) jsonv::parse(text, opts);
        }
        catch (const std::exception&)
        { }

        // Extraction from an index that failed to parse, which `jsonv::parse` never reaches because it validates
        // first. Nothing stops a caller doing this, and it is well defined -- `extract_tree` throws on a bad AST.
        try
        {
            (void) jsonv::parse_index::parse(text, opts).extract_tree();
        }
        catch (const std::exception&)
        { }
    }

    // The tape is sized from the source length by default, so its growth path almost never runs. Starting at a single
    // slot forces every reallocation.
    try
    {
        auto tight = jsonv::parse_index::parse(text,
                                               jsonv::parse_options::create_default(),
                                               std::optional<std::size_t>(1U)
                                              );
        (void) tight.extract_tree();
    }
    catch (const std::exception&)
    { }

    return 0;
}
