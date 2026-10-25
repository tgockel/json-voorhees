/** \file
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/value.hpp>
#include <jsonv/parse.hpp>

#include "test.hpp"
#include "allocation_counter.hpp"

#include <cstddef>
#include <cwchar>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <stdexcept>

TEST(parse_empty_string)
{
    jsonv::value val = jsonv::parse("\"\"");
    std::string_view str = val.as_string();
    ensure(str == "");
}

TEST(parse_single_backslash)
{
    jsonv::value val = jsonv::parse("\"\\\\\""); // "\\"
    std::string_view str = val.as_string();
    ensure(str == "\\");
    ensure_eq(1, val.size());
}

TEST(parse_bogus_string_throws)
{
    // Specify a Unicode escape that isn't long enough and doesn't use hex.
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\\uTT\""));
}

TEST(parse_strict_string_unprintables)
{
    auto options = jsonv::parse_options::create_strict();
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\t\"",   options));
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\b\"",   options));
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\f\"",   options));
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\n\"",   options));
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\r\"",   options));
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\x01\"", options));
}

TEST(string_comparisons)
{
    using namespace jsonv;
    
    value fire  = "fire";
    value wind  = "wind";
    value water = "water";
    value earth = "earth";
    value heart = "heart";
    
    ensure_eq(fire, fire);
    ensure_lt(fire, wind);
    ensure_lt(fire, water);
    ensure_gt(fire, earth);
    ensure_lt(fire, heart);
    
    ensure_gt(wind, fire);
    ensure_eq(wind, wind);
    ensure_gt(wind, water);
    ensure_gt(wind, earth);
    ensure_gt(wind, heart);
    
    ensure_gt(water, fire);
    ensure_lt(water, wind);
    ensure_eq(water, water);
    ensure_gt(water, earth);
    ensure_gt(water, heart);
    
    ensure_lt(earth, fire);
    ensure_lt(earth, wind);
    ensure_lt(earth, water);
    ensure_eq(earth, earth);
    ensure_lt(earth, heart);
    
    ensure_gt(heart, fire);
    ensure_lt(heart, wind);
    ensure_lt(heart, water);
    ensure_gt(heart, earth);
    ensure_eq(heart, heart);
}

TEST(wide_strings)
{
    using namespace jsonv;
    
    value narrow =  "some basic text";
    value wide   = L"some basic text";
    
    ensure_eq(narrow, wide);
    ensure(narrow.as_wstring() == wide.as_wstring());
}

TEST(wide_strings_astral)
{
    using namespace jsonv;

    // U+1F600. Round-tripping an astral code point out through as_wstring and back used to throw, because the
    // surrogate pair landed at the end of the wide string.
    value emoji = "\xf0\x9f\x98\x80";

    ensure_eq(emoji, value(emoji.as_wstring()));
}

#if WCHAR_MAX > 0xffff
TEST(wide_strings_wider_than_utf16)
{
    using namespace jsonv;

    // A wide string is UTF-16 even where wchar_t is 32 bits. U+1F600 as a single unit used to be stored as U+F600.
    const wchar_t raw[] = { wchar_t(0x1f600), L'\0' };

    ensure_throws(std::range_error, value(std::wstring(raw)));
    ensure_throws(std::range_error, value(raw));
}
#endif

TEST(string_view_construction)
{
    using namespace jsonv;

    value cp("Bob");
    value sv(cp.as_string_view());

    ensure_eq(cp, sv);
}

namespace
{

/// \a length bytes of text, with a NUL in it once it is long enough to hold one somewhere other than the ends.
std::string text_of_length(std::size_t length)
{
    std::string out;
    for (std::size_t idx = 0U; idx < length; ++idx)
        out += char('a' + idx % 26U);
    if (length > 3U)
        out[2] = '\0';
    return out;
}

}

// A string of up to 14 bytes is kept in the value itself, and a longer one in a block of its own. Either way the text
// reads back the same, however the value is copied, moved or swapped.
TEST(string_inline_and_out_of_line)
{
    for (std::size_t length : { 0U, 1U, 13U, 14U, 15U, 16U, 100U })
    {
        const std::string text = text_of_length(length);

        jsonv::value val(std::string_view{ text });
        ensure(val.as_string() == text);
        ensure(val.as_string_view() == text);
        ensure_eq(length, val.size());
        ensure_eq(length == 0U, val.empty());

        jsonv::value copy = val;
        ensure(copy.as_string() == text);
        ensure(copy.as_string().data() != val.as_string().data());

        jsonv::value moved = std::move(copy);
        ensure(copy.is_null());
        ensure(moved.as_string() == text);

        jsonv::value other = 5;
        other.swap(moved);
        ensure(other.as_string() == text);
        ensure_eq(5, moved.as_integer());

        moved = other;
        ensure(moved.as_string() == text);
        other = 6;
        ensure(moved.as_string() == text);

        ensure_eq(std::hash<std::string_view>()(text), std::hash<jsonv::value>()(val));
        ensure_eq(val, jsonv::value(text));
    }
}

TEST(string_parse_inline_boundary)
{
    for (std::size_t length : { 13U, 14U, 15U })
    {
        const std::string text(length, 'q');
        ensure(jsonv::parse("\"" + text + "\"").as_string() == text);
        ensure(jsonv::parse("[\"" + text + "\"]")[0].as_string() == text);
    }
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

// A string of up to 14 bytes takes no allocation, and a longer one takes one.
TEST(string_short_allocates_nothing)
{
    auto cost = [] (const std::string& text)
                {
                    jsonv_test::allocation_counter allocations;
                    jsonv::value                   val(std::string_view{ text });
                    return allocations.count();
                };

    ensure_eq(cost(text_of_length(0U)), cost(text_of_length(14U)));
    ensure_eq(cost(text_of_length(0U)) + 1U, cost(text_of_length(15U)));
    ensure_eq(cost(text_of_length(15U)), cost(text_of_length(1000U)));
}

#endif

