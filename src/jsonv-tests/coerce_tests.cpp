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

#include <jsonv/coerce.hpp>

#include <cmath>
#include <limits>

namespace jsonv_test
{

using namespace jsonv;

TEST(cant_coerce_corrupt)
{
    ensure(!can_coerce(static_cast<kind>(~0), kind::null));
}

TEST(coerce_object_valid)
{
    ensure(can_coerce(kind::object, kind::object));
    std::map<std::string, value> input = { { "x", 1 },
                                           { "y", 2 },
                                           { "z", "3" }
                                         };
    auto val = object(std::begin(input), std::end(input));
    auto output = coerce_object(val);
    ensure(input == output);
}

TEST(coerce_object_invalid)
{
    ensure(!can_coerce(kind::array, kind::object));
    ensure_throws(kind_error, coerce_object(array()));
    ensure_throws(kind_error, coerce_object("x"));
    ensure_throws(kind_error, coerce_object(1));
    ensure_throws(kind_error, coerce_object(1.2));
    ensure_throws(kind_error, coerce_object(null));
}

TEST(coerce_array_valid)
{
    std::vector<value> input = { 1, "blah", 5.6 };
    auto val = array(std::begin(input), std::end(input));
    auto output = coerce_array(val);
    ensure(input == output);
}

TEST(coerce_array_invalid)
{
    ensure_throws(kind_error, coerce_array(object()));
    ensure_throws(kind_error, coerce_array("x"));
    ensure_throws(kind_error, coerce_array(1));
    ensure_throws(kind_error, coerce_array(1.2));
    ensure_throws(kind_error, coerce_array(null));
}

TEST(coerce_string_valid)
{
    ensure_eq(coerce_string(null), "null");
    ensure_eq(coerce_string("blah"), "blah");
}

TEST(coerce_integer_null)
{
    ensure_throws(kind_error, coerce_integer(null));
}

TEST(coerce_integer_boolean)
{
    ensure_eq(0, coerce_integer(false));
    ensure_eq(1, coerce_integer(true));
}

TEST(coerce_integer_integer)
{
    ensure_eq(0, coerce_integer(0));
    ensure_eq(1, coerce_integer(1));
}

TEST(coerce_integer_decimal)
{
    ensure(can_coerce(kind::decimal, kind::integer));
    ensure_eq(0, coerce_integer(-0.2));
    ensure_eq(7, coerce_integer(7.8));
}

TEST(coerce_integer_decimal_clamp_max)
{
    ensure_eq(std::numeric_limits<std::int64_t>::max(), coerce_integer(18446744074709551600.0));
}

TEST(coerce_integer_decimal_clamp_min)
{
    ensure_eq(std::numeric_limits<std::int64_t>::min(), coerce_integer(-18446744074709551600.0));
}

// double(int64_t max) rounds up to 2^63, which is one past the largest representable int64_t, so
// this value has to clamp rather than convert.
TEST(coerce_integer_decimal_clamp_max_boundary)
{
    ensure_eq(std::numeric_limits<std::int64_t>::max(),
              coerce_integer(double(std::numeric_limits<std::int64_t>::max()))
             );
}

// double(int64_t min) is -2^63 exactly, so this one is representable and must convert unchanged.
TEST(coerce_integer_decimal_min_boundary)
{
    ensure_eq(std::numeric_limits<std::int64_t>::min(),
              coerce_integer(double(std::numeric_limits<std::int64_t>::min()))
             );
}

TEST(coerce_integer_decimal_infinity)
{
    ensure_eq(std::numeric_limits<std::int64_t>::max(),
              coerce_integer(std::numeric_limits<double>::infinity())
             );
    ensure_eq(std::numeric_limits<std::int64_t>::min(),
              coerce_integer(-std::numeric_limits<double>::infinity())
             );
}

TEST(coerce_integer_decimal_nan)
{
    ensure_eq(0, coerce_integer(std::numeric_limits<double>::quiet_NaN()));
}

TEST(coerce_integer_string_null)
{
    ensure_throws(kind_error, coerce_integer("null"));
}

TEST(coerce_integer_string_integer)
{
    ensure(can_coerce("0", kind::integer));
    ensure_eq(0, coerce_integer("0"));
    ensure_eq(1, coerce_integer("1"));
    
    ensure(!can_coerce("foo", kind::integer));
    ensure_throws(kind_error, coerce_integer("foo"));
}

TEST(coerce_integer_string_decimal)
{
    ensure(can_coerce("-0.2", kind::integer));
    ensure_eq(0, coerce_integer("-0.2"));
    ensure_eq(7, coerce_integer("7.8"));
}

TEST(coerce_integer_string_nested_valid)
{
    ensure_throws(kind_error, coerce_integer("\"5\""));
}

TEST(coerce_integer_string_decimal_clamp_max)
{
    ensure_eq(std::numeric_limits<std::int64_t>::max(), coerce_integer("18446744074709551600.0"));
}

TEST(coerce_integer_string_decimal_clamp_min)
{
    ensure_eq(std::numeric_limits<std::int64_t>::min(), coerce_integer("-18446744074709551600.0"));
}

// A string only reaches the decimal clamp in decimal form -- an integer literal past int64 max keeps
// its uint64 bits in the parser instead (see parse_tests.cpp) -- hence the ".0".
TEST(coerce_integer_string_decimal_clamp_max_boundary)
{
    ensure_eq(std::numeric_limits<std::int64_t>::max(), coerce_integer("9223372036854775808.0"));
}

TEST(coerce_integer_string_decimal_min_boundary)
{
    ensure_eq(std::numeric_limits<std::int64_t>::min(), coerce_integer("-9223372036854775808.0"));
}

TEST(coerce_decimal_null)
{
    ensure_throws(kind_error, coerce_decimal(null));
}

TEST(coerce_decimal_boolean)
{
    ensure_eq(0.0, coerce_decimal(false));
    ensure_eq(1.0, coerce_decimal(true));
}

TEST(coerce_decimal_integer)
{
    ensure(can_coerce(kind::integer, kind::decimal));
    ensure_eq(0.0, coerce_decimal(0));
    ensure_eq(1.0, coerce_decimal(1));
}

TEST(coerce_decimal_decimal)
{
    ensure_eq(-0.2, coerce_decimal(-0.2));
    ensure_eq(7.8, coerce_decimal(7.8));
}

TEST(coerce_decimal_string_null)
{
    ensure_throws(kind_error, coerce_decimal("null"));
}

TEST(coerce_decimal_string_integer)
{
    ensure(can_coerce("0", kind::decimal));
    ensure_eq(0.0, coerce_decimal("0"));
    ensure_eq(1.0, coerce_decimal("1"));
}

TEST(coerce_decimal_string_decimal)
{
    ensure(can_coerce("-0.2", kind::decimal));
    ensure_eq(-0.2, coerce_decimal("-0.2"));
    ensure_eq(7.8, coerce_decimal("7.8"));
    
    ensure(!can_coerce("foo", kind::decimal));
    ensure_throws(kind_error, coerce_decimal("foo"));
}

TEST(coerce_decimal_string_nested_valid)
{
    ensure_throws(kind_error, coerce_decimal("\"5.0\""));
}

TEST(coerce_boolean_null)
{
    ensure(!coerce_boolean(null));
}

TEST(coerce_boolean_boolean)
{
    ensure(!coerce_boolean(false));
    ensure(coerce_boolean(true));
}

TEST(coerce_boolean_ints)
{
    ensure(coerce_boolean(1));
    ensure(!coerce_boolean(0));
}

TEST(coerce_boolean_decimal)
{
    ensure(coerce_boolean(-4.3e9));
    ensure(!coerce_boolean(0.0));
}

TEST(coerce_boolean_string)
{
    ensure(coerce_boolean("something"));
    ensure(coerce_boolean("false"));
    ensure(!coerce_boolean(""));
}

TEST(coerce_boolean_object)
{
    ensure(coerce_boolean(object({{ "thing", 5 }})));
    ensure(!coerce_boolean(object()));
}

TEST(coerce_nulls)
{
    ensure(nullptr == coerce_null(null));
    ensure_throws(kind_error, coerce_null(object()));
    ensure_throws(kind_error, coerce_null(array()));
    ensure_throws(kind_error, coerce_null("string"));
    ensure_throws(kind_error, coerce_null(6));
    ensure_throws(kind_error, coerce_null(9.2));
    ensure_throws(kind_error, coerce_null(true));
    ensure_throws(kind_error, coerce_null(false));
}

}
