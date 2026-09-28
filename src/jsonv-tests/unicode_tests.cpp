/** \file
 *
 *  Copyright (c) 2012-2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"

#include <jsonv/value.hpp>
#include <jsonv/parse.hpp>

#include <string>
#include <string_view>

TEST(parse_unicode_single)
{
    std::string s = jsonv::parse("\"\\u0004\"").as_string();
    ensure(s.size() == 1);
    ensure(s[0] == '\x04');
}

TEST(parse_unicode_inline)
{
    std::string s = jsonv::parse("\"é\"").as_string();
    ensure_eq(s.size(), 2U);
    ensure(s[0] == '\xc3');
    ensure(s[1] == '\xa9');
}

TEST(parse_unicode_multi)
{
    std::string s = jsonv::parse("\"\\u00e9\"").as_string();
    ensure(s.size() == 2);
    ensure(s[0] == '\xc3');
    ensure(s[1] == '\xa9');
}

TEST(parse_unicode_insanity)
{
    std::string s = jsonv::parse("\"\\uface\"").as_string();
    ensure(s.size() == 3);
    // The right answer according to Python: u'\uface'.encode('utf-8')
    const char vals[] = "\xef\xab\x8e";
    for (unsigned idx = 0; idx < 3; ++idx)
        ensure(s[idx] == vals[idx]);
}

TEST(parse_unicode_invalid_surrogates)
{
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\\udead\\ubeef\"").as_string());
}

static constexpr auto k_utf8        = jsonv::parse_options::encoding::utf8;
static constexpr auto k_utf8_strict = jsonv::parse_options::encoding::utf8_strict;

/// Comments are on so that a string behind one is still reached, rather than the parse stopping at the `/`.
static jsonv::parse_options options_for(jsonv::parse_options::encoding encoding)
{
    return jsonv::parse_options().string_encoding(encoding).comments(true);
}

/// `utf8_strict` differs from `utf8` only in refusing unprintable ASCII, so UTF-8 conformance has to hold in both. A
/// macro rather than a function so that a failure reports the line of the input which caused it.
#define JSONV_TEST_ENSURE_UTF8_REJECTED(text_)                                                  \
    do                                                                                          \
    {                                                                                           \
        ensure_throws(jsonv::parse_error, jsonv::parse(text_, options_for(k_utf8)));            \
        ensure_throws(jsonv::parse_error, jsonv::parse(text_, options_for(k_utf8_strict)));     \
    } while (false)

TEST(parse_utf8_rejects_ill_formed)
{
    // The ill-formed `utf8_*` seeds in `src/jsonv-fuzz/corpus`, byte for byte (#207).
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xf8\x80\x80\x80\x80\"]");            // 5 byte form, removed by RFC 3629
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xfc\x80\x80\x80\x80\x80\"]");        // 6 byte form, likewise
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xc0\x80\"]");                        // overlong U+0000
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xed\xa0\x80\"]");                    // U+D800, a surrogate
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xf5\x80\x80\x80\"]");                // U+140000, above U+10FFFF
    JSONV_TEST_ENSURE_UTF8_REJECTED("[/*\xd6\xd0*/\"\xfc\x80\x80\x80\x80\x80\"]"); // ...behind a comment

    // Either side of each limit Table 3-7 places on the byte after the lead.
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xc1\xbf\"]");                        // overlong U+007F
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xe0\x80\x80\"]");                    // overlong U+0000
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xe0\x9f\xbf\"]");                    // overlong U+07FF
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xed\xbf\xbf\"]");                    // U+DFFF, a surrogate
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xf0\x80\x80\x80\"]");                // overlong U+0000
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xf0\x8f\xbf\xbf\"]");                // overlong U+FFFF
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xf4\x90\x80\x80\"]");                // U+110000
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xf7\xbf\xbf\xbf\"]");                // U+1FFFFF
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xfe\"]");
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\xff\"]");

    // The same check in a key, in a string which also needs unescaping, and past the chunks the SIMD scan consumes.
    JSONV_TEST_ENSURE_UTF8_REJECTED("{\"\xed\xa0\x80\": 1}");
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"\\n\xed\xa0\x80\"]");
    JSONV_TEST_ENSURE_UTF8_REJECTED("[\"0123456789012345678901234567890123456789\xed\xa0\x80\"]");
}

TEST(parse_utf8_surrogate_escaped_and_raw_agree)
{
    // One codepoint, spelled two ways. Only the escape used to be refused (#207).
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\\uD800\""));
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\xed\xa0\x80\""));
}

TEST(parse_utf8_accepts_well_formed_boundaries)
{
    // The first and last codepoint of each sequence length, the codepoints either side of the surrogates, and an emoji
    // for something outside the BMP which is not at an edge.
    const std::string_view encodings[] =
        {
            "\xc2\x80",         // U+0080
            "\xdf\xbf",         // U+07FF
            "\xe0\xa0\x80",     // U+0800
            "\xed\x9f\xbf",     // U+D7FF
            "\xee\x80\x80",     // U+E000
            "\xef\xbf\xbf",     // U+FFFF
            "\xf0\x90\x80\x80", // U+10000
            "\xf0\x9f\x98\x80", // U+1F600
            "\xf4\x8f\xbf\xbf", // U+10FFFF
        };

    for (std::string_view utf8 : encodings)
    {
        const std::string json = "\"" + std::string(utf8) + "\"";
        for (auto encoding : { k_utf8, k_utf8_strict })
        {
            jsonv::value val = jsonv::parse(json, options_for(encoding));
            ensure_eq(std::string(utf8), val.as_string());

            // The encoder writes this as a `\u` escape, or as a surrogate pair outside the BMP.
            ensure_eq(val, jsonv::parse(jsonv::to_string(val), options_for(encoding)));
        }
    }
}
