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
#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <map>
#include <string>

#include <json-voorhees/value.hpp>
#include <json-voorhees/parse.hpp>

#include "test.hpp"

TEST(parse_unicode_single)
{
    std::string s = jsonv::parse("\"\\u0004\"").as_string();
    ensure(s.size() == 1);
    ensure(s[0] == '\x04');
}

TEST(parse_unicode_inline)
{
    std::string s = jsonv::parse("\"é\"").as_string();
    ensure(s.size() == 2);
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
    // This is technically an invalid Unicode because of some unpaired surrogate...apparently 0xd800 - 0xdfff is a magic
    // range for something I don't understand. Anyway, this library will parse and translate it to UTF8 (perhaps not in
    // a valid way). My shell doesn't seem to have a problem printing a ? for this.
    std::string s = jsonv::parse("\"\\udead\\ubeef\"").as_string();
    ensure(s.size() == 6);
    // The right answer according to Python: u'\udead\ubeef'.encode('utf-8')
    const char vals[] = "\xed\xba\xad\xeb\xbb\xaf";
    for (unsigned idx = 0; idx < sizeof vals; ++idx)
        ensure(s[idx] == vals[idx]);
}

TEST(demo)
{
    std::string src = "{ \"blazing\": [ 3 \"\\\"\\n\" 4.5 5.123 4.10921e19 ] " // <- no need for commas
                      "  \"text\": [ 1, 2, ,,3,, 4,,, \t\"something\"],,  ,, " // <- or go crazy with commas
                      "  \"we call him \\\"empty array\\\"\": [] "
                      "  \"we call him \\\"empty object\\\"\": {} "
                      "  \"unicode\" :\"\\uface\""
                      "}";
    jsonv::value parsed = jsonv::parse(src);
    std::cout << parsed;
}

int main(int, char**)
{
    int fail_count = 0;
    for (auto iter = jsonv_test::get_unit_tests().begin(); iter != jsonv_test::get_unit_tests().end(); ++iter)
    {
        if (!(*iter)->run())
            ++fail_count;
    }
    
    return fail_count;
}
