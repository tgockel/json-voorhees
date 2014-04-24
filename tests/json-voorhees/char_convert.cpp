/** \file
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"

#include <json-voorhees/char_convert.hpp>

template <std::size_t N>
static std::string string_decode_static(const char (& data)[N])
{
    // Use N-1 for length to not include the '\0' at the end
    return jsonv::detail::string_decode(data, N-1);
}

TEST(string_decode_unchanged)
{
    ensure_eq("Hello!", string_decode_static("Hello!"));
}

TEST(string_decode_utf_one_char)
{
    ensure_eq("\xe2\x98\xa2", string_decode_static("\\u2622"));
}

TEST(string_decode_utf_char_starts)
{
    ensure_eq("\xe2\x98\xa2normal text", string_decode_static("\\u2622normal text"));
}

TEST(string_decode_utf_char_ends)
{
    ensure_eq("normal text\xe2\x98\xa2", string_decode_static("normal text\\u2622"));
}

TEST(string_decode_utf_char_bookends)
{
    ensure_eq("\xe2\x98\xa2normal text\xe2\x9d\xa4", string_decode_static("\\u2622normal text\\u2764"));
}

TEST(string_decode_utf_char_surrounded)
{
    ensure_eq("normal\xe2\x98\xa2text", string_decode_static("normal\\u2622text"));
}

TEST(string_decode_utf_many_chars)
{
    ensure_eq("\xe2\x9d\xa4 \xe2\x98\x80 \xe2\x98\x86 \xe2\x98\x82 \xe2\x98\xbb \xe2\x99\x9e \xe2\x98\xaf \xe2\x98\xad \xe2\x98\xa2 \xe2\x82\xac \xe2\x86\x92 \xe2\x98\x8e \xe2\x9d\x84 \xe2\x99\xab \xe2\x9c\x82 \xe2\x96\xb7 \xe2\x9c\x87 \xe2\x99\x8e \xe2\x87\xa7 \xe2\x98\xae \xe2\x99\xbb \xe2\x8c\x98 \xe2\x8c\x9b \xe2\x98\x98",
              string_decode_static("\\u2764 \\u2600 \\u2606 \\u2602 \\u263b \\u265e \\u262f \\u262d \\u2622 \\u20ac \\u2192 \\u260e \\u2744 \\u266b \\u2702 \\u25b7 \\u2707 \\u264e \\u21e7 \\u262e \\u267b \\u2318 \\u231b \\u2618"));
}
