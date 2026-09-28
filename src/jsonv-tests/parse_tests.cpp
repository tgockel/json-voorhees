/// \file
///
/// Copyright (c) 2014-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/ast.hpp>
#include <jsonv/parse.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace jsonv;

#define TEST_PARSE(name) TEST(parse_ ## name)

TEST_PARSE(number_0)
{
    value result = parse("0");
    ensure_eq(value(0), result);
}

TEST_PARSE(number_negative_0)
{
    value result = parse("-0");
    ensure_eq(value(0), result);
}

TEST_PARSE(number_double_zero)
{
    ensure_throws(parse_error, parse("00"));
}

TEST_PARSE(number_negative_double_zero)
{
    ensure_throws(parse_error, parse("-00"));
}

TEST_PARSE(number_leading_zero)
{
    ensure_throws(parse_error, parse("013"));
}

TEST_PARSE(number_decimal_underflow)
{
    value result = parse("[1e-10000]");
    auto parsed = result.at(0).as_decimal();
    ensure_eq(0.0, parsed);
    ensure(!std::signbit(parsed));
}

TEST_PARSE(number_decimal_underflow_large_exponents)
{
    value result = parse("[1e-214748363, 1e-214748364]");
    ensure_eq(0.0, result.at(0).as_decimal());
    ensure_eq(0.0, result.at(1).as_decimal());
}

TEST_PARSE(number_decimal_negative_underflow)
{
    value result = parse("[-1e-10000]");
    auto parsed = result.at(0).as_decimal();
    ensure_eq(0.0, parsed);
    ensure(std::signbit(parsed));
}

TEST_PARSE(number_decimal_overflow_still_throws)
{
    ensure_throws(std::invalid_argument, parse("[1e10000]"));
}

// Integer extraction takes a slow path for magnitudes too large for the SWAR parser. That path used
// to call `strtoll`/`strtoull`, which scan until they reach a non-digit -- but the token points into
// the source text, which is a `string_view` and not NUL-terminated. Parsing a view whose end is
// immediately followed by more digits therefore read past it and produced the wrong value. The
// digits beyond `length` below must be ignored entirely.
static value parse_prefix(std::string_view source, std::size_t length)
{
    return parse(source.substr(0, length));
}

TEST_PARSE(number_integer_slow_path_stops_at_view_end)
{
    // 20 digits: too large for int64, but representable in uint64, so it wraps (see the note in
    // `ast_node::integer::value`). The trailing "12345" must not be consumed.
    ensure_eq(static_cast<std::int64_t>(12345678901234567890ULL),
              parse_prefix("1234567890123456789012345", 20).as_integer()
             );
}

TEST_PARSE(number_integer_slow_path_stops_at_view_end_negative)
{
    // 19 magnitude digits exceeds the negative SWAR limit of 18, so this takes the slow path too.
    ensure_eq(std::int64_t(-1234567890123456789),
              parse_prefix("-123456789012345678901234", 20).as_integer()
             );
}

TEST_PARSE(number_integer_slow_path_stops_at_view_end_at_uint64_max)
{
    // `UINT64_MAX` with another digit after the view. Reading that digit would put the magnitude beyond 64 bits, which
    // used to saturate to `UINT64_MAX` anyway and hide the overrun; now it would come back as a decimal.
    value result = parse_prefix("184467440737095516159", 20);
    ensure(result.kind() == kind::integer);
    ensure_eq(std::int64_t(-1), result.as_integer());
}

TEST_PARSE(number_integer_at_end_of_input)
{
    // The same tokens with nothing after them at all -- the case that read past the allocation.
    ensure_eq(static_cast<std::int64_t>(12345678901234567890ULL), parse("12345678901234567890").as_integer());
    ensure_eq(std::int64_t(-1234567890123456789),                 parse("-1234567890123456789").as_integer());
}

TEST_PARSE(number_integer_boundaries)
{
    ensure_eq(std::numeric_limits<std::int64_t>::max(), parse("9223372036854775807").as_integer());
    ensure_eq(std::numeric_limits<std::int64_t>::min(), parse("-9223372036854775808").as_integer());

    // 2^63 fits in a uint64 and is stored with its bits intact, which reads back as INT64_MIN.
    ensure_eq(std::numeric_limits<std::int64_t>::min(), parse("9223372036854775808").as_integer());
    ensure_eq(std::int64_t(-1),                         parse("18446744073709551615").as_integer());
}

TEST_PARSE(number_integer_beyond_64_bits_is_decimal)
{
    // There are no bits to keep for these, so they are the nearest `double` rather than the bound they used to be
    // saturated to -- which made `18446744073709551616` indistinguishable from `UINT64_MAX` (#206). Each expectation
    // is the same digits as a C++ literal, which the compiler rounds correctly.
    value just_past = parse("18446744073709551616");
    ensure(just_past.kind() == kind::decimal);
    ensure_eq(18446744073709551616.0, just_past.as_decimal());
    ensure_throws(kind_error, just_past.as_integer());

    value far_past = parse("12345678901234567890123");
    ensure(far_past.kind() == kind::decimal);
    ensure_eq(12345678901234567890123.0, far_past.as_decimal());

    value just_below = parse("-9223372036854775809");
    ensure(just_below.kind() == kind::decimal);
    ensure_eq(-9223372036854775809.0, just_below.as_decimal());

    value far_below = parse("-99999999999999999999999");
    ensure(far_below.kind() == kind::decimal);
    ensure_eq(-99999999999999999999999.0, far_below.as_decimal());

    value mixed = parse("[1, 18446744073709551616]");
    ensure(mixed.at(0).kind() == kind::integer);
    ensure(mixed.at(1).kind() == kind::decimal);

    // A magnitude past what a `double` holds is refused just as `1e10000` is -- as the integer it was written as.
    try
    {
        (void) parse(std::string(400, '9'));
        ensure(!"std::invalid_argument was not thrown");
    }
    catch (const std::invalid_argument& ex)
    {
        ensure(std::string_view(ex.what()).starts_with("Failed to extract integer"));
    }
}

TEST_PARSE(number_integer_node_beyond_64_bits_throws)
{
    // The node's own accessor returns a `std::int64_t`, so it has no `double` to fall back on.
    for (std::string_view token : { "18446744073709551616", "-9223372036854775809" })
        ensure_throws(std::invalid_argument, ast_node::integer(token.data(), token.size()).value());
}

static const value simple_obj = object({ { "foo", 4 },
                                         { "bar", array({ 2, 3, 4, "5" }) },
                                         { "raz", object() }
                                       });

TEST_PARSE(object_simple_no_spaces)
{
    value result = parse("{\"foo\":4,\"bar\":[2,3,4,\"5\"],\"raz\":{}}");
    ensure_eq(simple_obj, result);
}

TEST_PARSE(object_simple_no_newlines)
{
    value result = parse("{\"foo\": 4, \"raz\": {   }, \"bar\": [ 2, 3, 4, \"5\"]}");
    ensure_eq(simple_obj, result);
}

TEST_PARSE(object_simple_spaces_and_tabs)
{
    value result = parse("           {    \t       \"foo\" :                 \t            4  , "
                         " \"bar\"                                :               [          \t         2\t,"
                         " 3\t\t\t,\t4                ,                    \"5\"             ],"
                         "         \t\"raz\"      \t: {                                                   }"
                         " \t\t}            \t");
    ensure_eq(simple_obj, result);
}

TEST_PARSE(object_simple_newlines)
{
    value result = parse(R"({
    "foo":4,
    "bar":[2,3,4,"5"],
    "raz":{}
})");
    ensure_eq(simple_obj, result);
}

TEST_PARSE(object_simple_general_havoc)
{
    value result = parse(R"(
        {
                                "foo"
:
                4,"raz"
                                                                        :
{




        },"bar"       :
              [
               2,
                         3,4,
                      "5"
]}


    )");
    ensure_eq(simple_obj, result);
}

TEST_PARSE(object_nested_single)
{
    value result = parse(R"({"a": {"b": 10}, "c":25})");
    value expected = object({ { "a", object({ { "b", 10 } }) },
                              { "c", 25 }
                           });
    ensure_eq(expected, result);
}

TEST_PARSE(object_empties_in_array)
{
    value result = parse(R"({"a": {"b": 10}, "c": 23.9, "d": [{"e": {}, "f": 41.4, "g": null, "h": 5}, {"i":null}]})");
    value expected = object({ { "a", object({ { "b", 10 } }) },
                              { "c", 23.9 },
                              { "d", array({ object({ { "e", object() },
                                                      { "f", 41.4 },
                                                      { "g", null },
                                                      { "h", 5 },
                                                   }),
                                             object({ { "i", null } })
                                          })
                              }
                           });
    ensure_eq(expected, result);
}

TEST_PARSE(empty_object_char_ptr_range)
{
    const char buff[] = "{}";
    value result = parse(buff + 0, buff + sizeof buff);
    value expected = object();
    ensure_eq(expected, result);
}

TEST_PARSE(null)
{
    value result = parse("null");
    value expected = value(null);
    ensure_eq(expected, result);
}

TEST_PARSE(null_in_arr)
{
    value result = parse("[null,4]");
    value expected = array({ null, 4 });
    ensure_eq(expected, result);
}

TEST_PARSE(null_in_obj)
{
    value result = parse(R"({"a": null})");
    value expected = object({ { "a", null } });
    ensure_eq(expected, result);
}

TEST_PARSE(malformed_bools)
{
    ensure_throws(parse_error, parse("truish"));
    ensure_throws(parse_error, parse("tru"));
    ensure_throws(parse_error, parse("falsy"));
}

TEST_PARSE(malformed_nulls)
{
    ensure_throws(parse_error, parse("nul"));
}

TEST_PARSE(object_in_array)
{
    value result = parse(R"({"a": null, "b": {"c": 1, "d": "e", "f": null, "g": 2, )"
                         R"("h": [{"i": 3, "j": null, "k": "l", "m": 4, "n": "o", "p": "q", "r": 5}, )"
                         R"({"s": 6, "t": 7, "u": null}, {"v": "w"}]}})"
                        );
    value expected = object({ { "a", null },
                              { "b", object({ { "c", 1 },
                                              { "d", "e" },
                                              { "f", null },
                                              { "g", 2 },
                                              { "h", array({ object({ { "i", 3 },
                                                                      { "j", null },
                                                                      { "k", "l" },
                                                                      { "m", 4 },
                                                                      { "n", "o" },
                                                                      { "p", "q" },
                                                                      { "r", 5 },
                                                                   }),
                                                             object({ { "s", 6 },
                                                                      { "t", 7 },
                                                                      { "u", null },
                                                                   }),
                                                             object({ { "v", "w" } })
                                                          })
                                              },
                                           })
                              }
                           });
    ensure_eq(expected, result);
}

TEST_PARSE(malformed_decimal)
{
    ensure_throws(jsonv::parse_error, parse("123.456.789"));
}

TEST_PARSE(malformed_decimal_in_object)
{
    ensure_throws(jsonv::parse_error, parse(R"({"x": 123.456.789 })"));
}

TEST_PARSE(malformed_string_unterminated)
{
    ensure_throws(jsonv::parse_error, parse(R"("abc)"));
    ensure_throws(jsonv::parse_error, parse(R"(")"));
}

TEST_PARSE(malformed_boolean)
{
    ensure_throws(jsonv::parse_error, parse("try"));
}

TEST_PARSE(depth)
{
    std::string src = R"({"a": null, "b": [{}, 3, 4.5, false, [[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]})";
    // this isn't all that useful -- we just want to ensure that the normal src parses
    (void) parse(src);
    ensure_throws(parse_error, parse(src, parse_options::create_strict()));
}

TEST_PARSE(literal)
{
    value v = "[1, 2, 3, 4]"_json;
    ensure_eq(array({ 1, 2, 3, 4 }), v);
}

TEST_PARSE(comments_invalid_leading_slash_then_bogus)
{
    ensure_throws(parse_error, parse("{}/1", parse_options().comments(true)));
}

TEST_PARSE(malformed_comment_complete)
{
    ensure_throws(parse_error, parse("/1", parse_options().comments(true)));
}

TEST_PARSE(malformed_comment_in_object)
{
    ensure_throws(parse_error, parse(R"({"a": ////////"b"})", parse_options().comments(true)));
}

TEST_PARSE(comment_in_object)
{
    value val = parse(R"({"a": /* yo */"b"})", parse_options().comments(true));
    ensure_eq(object({ { "a", "b" } }), val);
}

TEST_PARSE(comment_in_array)
{
    value val = parse(R"(["a", /* yo */"b"])", parse_options().comments(true));
    ensure_eq(array({ "a", "b" }), val);
}

TEST_PARSE(comment_rejected_by_default)
{
    ensure_throws(parse_error, parse(R"({"a": /* yo */"b"})"));
    ensure_throws(parse_error, parse(R"(["a", /* yo */"b"])"));
}

TEST_PARSE(invalid_utf8_input)
{
    ensure_throws(jsonv::parse_error, jsonv::parse("\"\xe4\""));
}
