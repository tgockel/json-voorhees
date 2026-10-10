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
#include "locale_util.hpp"

#include <jsonv/encode.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/value.hpp>
#include <jsonv/writer.hpp>

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

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

// The stream is the caller's, so when its contents reach wherever they are going is too. A flush per line makes pretty
// output to a file or socket cost a write per line.
TEST(encode_pretty_does_not_flush)
{
    struct sync_counting_buf : std::stringbuf
    {
        int syncs = 0;

        int sync() override
        {
            ++syncs;
            return std::stringbuf::sync();
        }
    };

    sync_counting_buf buf;
    std::ostream      os(&buf);
    jsonv::ostream_pretty_encoder(os).encode(jsonv::parse(k_some_json));
    ensure(buf.str().find('\n') != std::string::npos);
    ensure_eq(0, buf.syncs);
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

// A number inserted into a stream picks up its locale's digit grouping. `1,234,567` is not JSON, and inside an array
// the separators read back as extra elements. See issue #330.
TEST(encode_integer_ignores_global_locale)
{
    global_grouping_locale grouping;

    // The facet really is in effect, or nothing below proves anything.
    std::ostringstream probe;
    probe << 1000;
    ensure_eq(std::string("1,000"), probe.str());

    ensure_eq(std::string("1234567"), jsonv::to_string(jsonv::value(std::int64_t(1234567))));
    ensure_encodes_as("[1234567,-89012345]", "[1234567,-89012345]");
    ensure_encodes_as("[-9223372036854775808,9223372036854775807]", "[-9223372036854775808,9223372036854775807]");

    // The pretty encoder delegates integer formatting to `ostream_encoder`, so it has to agree.
    const jsonv::value original = jsonv::parse("[1234567,-89012345]");
    std::ostringstream pretty;
    jsonv::ostream_pretty_encoder(pretty).encode(original);
    ensure_eq(original, jsonv::parse(pretty.str()));

    // Decimals already went through `format_decimal`; pin them beside integers.
    ensure_encodes_as("[1234.5,-5678.25]", "[1234.5,-5678.25]");
}

TEST(encode_integer_ignores_stream_locale)
{
    std::ostringstream os;
    os.imbue(std::locale(std::locale::classic(), new grouping_numpunct));
    os << jsonv::value(std::int64_t(1234567));
    ensure_eq(std::string("1234567"), os.str());
}

// Written unformatted, an integer would leave a pending width for the `,` after it, which pads to `0,` and reads back as
// `[120,34]`.
TEST(encode_integer_value_survives_pending_width)
{
    std::ostringstream os;
    jsonv::writer w(os);
    w.array_begin();
    os << std::setfill('0') << std::setw(2);
    w.integer(12).integer(34).array_end();
    ensure_eq(std::string("[12,34]"), os.str());
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

TEST(encode_invalid_utf8_uses_replacement_for_ill_formed_sequences)
{
    // Each of these has every continuation byte in place, so the encoder used to decode it and write out the codepoint:
    // an overlong U+0000 came out as `\u0000`, changing the value, and a raw surrogate came out as `\ud800`, which the
    // parser refuses. None of them is well-formed UTF-8, so each byte is now replaced on its own like any other
    // malformed input, and the output parses (#207).
    const std::pair<const char*, const char*> cases[] =
        {
            { "N\xc0\x80X",             "\"N\\u00c0\\u0080X\""                     },
            { "N\xed\xa0\x80X",         "\"N\\u00ed\\u00a0\\u0080X\""               },
            { "N\xf5\x80\x80\x80X",     "\"N\\u00f5\\u0080\\u0080\\u0080X\""         },
            { "N\xf8\x80\x80\x80\x80X", "\"N\\u00f8\\u0080\\u0080\\u0080\\u0080X\"" },
        };

    for (const auto& [input, expected] : cases)
    {
        std::string output = jsonv::to_string(jsonv::value(input));
        ensure_eq(output, expected);
        ensure(jsonv::parse(output).kind() == jsonv::kind::string);
    }
}

/// Encode \a val through an \c ostream_encoder with \c ensure_ascii set to \a ensure_ascii.
static std::string encode_with_ensure_ascii(const jsonv::value& val, bool ensure_ascii)
{
    std::ostringstream ss;
    jsonv::ostream_encoder encoder(ss);
    encoder.ensure_ascii(ensure_ascii);
    encoder.encode(val);
    return ss.str();
}

TEST(encode_ensure_ascii_off_passes_utf8_through)
{
    // A 2 byte sequence, and a 4 byte one from outside the BMP.
    const std::string  text = "Travis G\xc3\xb6" "ckel \xf0\x9f\x98\x80";
    const jsonv::value val  = text;

    const std::string raw = encode_with_ensure_ascii(val, false);
    ensure_eq(raw, "\"" + text + "\"");
    ensure_eq(val, jsonv::parse(raw));

    const std::string escaped = encode_with_ensure_ascii(val, true);
    ensure_eq(escaped, "\"Travis G\\u00f6ckel \\ud83d\\ude00\"");
    ensure_eq(val, jsonv::parse(escaped));

    // The pretty encoder writes strings through `ostream_encoder`, so it follows the same setting.
    std::ostringstream pretty;
    jsonv::ostream_pretty_encoder pretty_encoder(pretty);
    pretty_encoder.ensure_ascii(false);
    pretty_encoder.encode(jsonv::array({ val }));
    ensure(pretty.str().find(text) != std::string::npos);
    ensure_eq(jsonv::array({ val }), jsonv::parse(pretty.str()));
}

TEST(encode_ensure_ascii_off_still_escapes_controls_and_malformed_utf8)
{
    // Only well-formed multi-byte UTF-8 goes through as it is. The output has to stay JSON, so controls are escaped
    // (#273), and malformed bytes are replaced one at a time just as they are with `ensure_ascii` on (#207).
    const std::string raw = encode_with_ensure_ascii(jsonv::value("a\x01" "b\x7f" "c\xc0\x80"), false);
    ensure_eq(raw, "\"a\\u0001b\\u007fc\\u00c0\\u0080\"");

    const auto strict = jsonv::parse_options().string_encoding(jsonv::parse_options::encoding::utf8_strict);
    ensure(jsonv::parse(raw, strict).kind() == jsonv::kind::string);
}

TEST(encode_ensure_ascii_can_be_turned_back_on)
{
    const jsonv::value val = "G\xc3\xb6" "ckel";

    std::ostringstream ss;
    jsonv::ostream_encoder encoder(ss);
    encoder.ensure_ascii(false);
    encoder.encode(val);
    encoder.ensure_ascii(true);
    encoder.encode(val);
    ensure_eq(ss.str(), "\"G\xc3\xb6" "ckel\"\"G\\u00f6ckel\"");
}

}
