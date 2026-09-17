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

#include <jsonv/encode.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/value.hpp>

#include <cmath>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>

namespace jsonv_test
{

namespace
{

void ensure_encodes_as(std::string_view input, std::string_view expected)
{
    ensure_eq(std::string(expected), jsonv::to_string(jsonv::parse(input)));
}

}

static const char k_some_json[] = R"({
  "a": [ 4, 5, 6, [7, 8, 9, {"something": 5, "else": 6}]],
  "b": "blah",
  "c": { "baz": ["bazar"], "cat": ["Eric", "Bob"] },
  "d": {},
  "e": [],
  "f": null,
  "g": [ true, false ]
})";

TEST(encode_pretty_print)
{
    auto val = jsonv::parse(k_some_json);
    jsonv::ostream_pretty_encoder encoder(std::cout);
    encoder.encode(val);
}

TEST(encode_nan)
{
    auto val = jsonv::parse(k_some_json);
    val.at_path(".a[2]") = std::nan("");
    std::ostringstream ss;
    ss << val;
    
    std::string str = ss.str();
    auto decoded = jsonv::parse(str);
    ensure_ne(val, decoded);
    
    // change val to have null in place of the NaN
    val.at_path(".a[2]") = jsonv::null;
    ensure_eq(val, decoded);
}

TEST(encode_decimal_shortest_roundtrip)
{
    ensure_encodes_as("[5e-324]", "[5e-324]");
    ensure_encodes_as("[2.225073858507201e-308]", "[2.225073858507201e-308]");
    ensure_encodes_as("[2.2250738585072014e-308]", "[2.2250738585072014e-308]");
    ensure_encodes_as("[1.7976931348623157e308]", "[1.7976931348623157e+308]");
}

TEST(encode_decimal_keeps_decimal_kind)
{
    // `std::to_chars` with `chars_format::general` emits the shortest round-trip form, which for an integral value has
    // no decimal point or exponent. Left alone, those tokens re-parse as `kind::integer`. See issue #208.
    ensure_encodes_as("[2.0]",  "[2.0]");
    ensure_encodes_as("[0.0]",  "[0.0]");
    ensure_encodes_as("[-0.0]", "[-0.0]");
    ensure_encodes_as("[-2.0]", "[-2.0]");
    ensure_encodes_as("[1e2]",  "[100.0]");

    // Values that already carry a point or an exponent are untouched.
    ensure_encodes_as("[1.5]",                   "[1.5]");
    ensure_encodes_as("[1.7976931348623157e308]", "[1.7976931348623157e+308]");
    ensure_encodes_as("[5e-324]",                 "[5e-324]");

    // Integers stay integers.
    ensure_encodes_as("[2]",  "[2]");
    ensure_encodes_as("[-0]", "[0]");
}

namespace
{

void ensure_decimal_round_trips(std::string_view source, bool expect_negative_zero)
{
    jsonv::value      original = jsonv::parse(source);
    const std::string encoded  = jsonv::to_string(original);
    jsonv::value      reparsed = jsonv::parse(encoded);

    ensure(reparsed.at(0).kind() == jsonv::kind::decimal);

    const double decimal = reparsed.at(0).as_decimal();
    ensure_eq(original.at(0).as_decimal(), decimal);
    ensure_eq(expect_negative_zero, std::signbit(decimal) && decimal == 0.0);

    // Encoding must be a fixed point.
    ensure_eq(encoded, jsonv::to_string(reparsed));

    // The pretty encoder delegates decimal formatting to `ostream_encoder`, so it has to agree.
    std::ostringstream pretty;
    jsonv::ostream_pretty_encoder(pretty).encode(original);
    ensure(jsonv::parse(pretty.str()).at(0).kind() == jsonv::kind::decimal);
}

}

TEST(encode_decimal_negative_zero_round_trip)
{
    ensure_decimal_round_trips("[-0.0]", true);
}

TEST(encode_decimal_negative_underflow_round_trip)
{
    // `-1e-10000` underflows to negative zero; `parse_number_decimal_negative_underflow` pins the parse side of this,
    // and the sign has to survive the encode side too.
    ensure_decimal_round_trips("[-1e-10000]", true);
}

TEST(encode_decimal_integral_round_trip)
{
    ensure_decimal_round_trips("[0.0]", false);
    ensure_decimal_round_trips("[2.0]", false);
    ensure_decimal_round_trips("[-2.0]", false);
    ensure_decimal_round_trips("[1.5]", false);
}

TEST(encode_invalid_utf8_uses_replacement_for_bogus_2_byte)
{
    jsonv::value val = "N\xc1pX";
    std::string output = jsonv::to_string(val);
    ensure_eq(output, "\"N\\u00c1pX\"");
}

TEST(encode_invalid_utf8_uses_replacement_for_bogus_3_byte)
{
    jsonv::value val = "N\xe0hiX";
    std::string output = jsonv::to_string(val);
    ensure_eq(output, "\"N\\u00e0hiX\"");
}

TEST(encode_invalid_utf8_uses_replacement_for_bogus_4_byte)
{
    jsonv::value val = "N\xf0himX";
    std::string output = jsonv::to_string(val);
    ensure_eq(output, "\"N\\u00f0himX\"");
}

TEST(encode_invalid_utf8_uses_replacement_for_bogus_5_byte)
{
    jsonv::value val = "N\xf9himsX";
    std::string output = jsonv::to_string(val);
    ensure_eq(output, "\"N\\u00f9himsX\"");
}

TEST(encode_invalid_utf8_uses_replacement_for_bogus_6_byte)
{
    jsonv::value val = "N\xfchimsiX";
    std::string output = jsonv::to_string(val);
    ensure_eq(output, "\"N\\u00fchimsiX\"");
}

TEST(encode_invalid_utf8_uses_replacement_for_bogus_mask)
{
    jsonv::value val = "N\xffR\xfeX";
    std::string output = jsonv::to_string(val);
    ensure_eq(output, "\"N\\u00ffR\\u00feX\"");
}

TEST(encode_invalid_utf8_uses_replacement_at_end)
{
    jsonv::value val = "\xe8";
    std::string output = jsonv::to_string(val);
    ensure_eq(output, "\"\\u00e8\"");
}

}
