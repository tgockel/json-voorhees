/** \file
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"

#include <jsonv/algorithm.hpp>
#include <jsonv/value.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

// Taken by value so the two are distinct objects: the same-address shortcut in compare cannot hide a regression.
void check_comparison(const value left, const value right, int expected)
{
    ensure_eq(compare(left, right), expected);
    ensure_eq(left.compare(right), expected);
    ensure_eq(compare_icase(left, right), expected);
    ensure_eq(left == right, expected == 0);
    ensure_eq(left != right, expected != 0);
    ensure_eq(left < right, expected < 0);
    ensure_eq(left <= right, expected <= 0);
    ensure_eq(left > right, expected > 0);
    ensure_eq(left >= right, expected >= 0);
}

void check_decimal_comparison(double a, double b, int expected)
{
    ensure_eq(compare_traits::compare_decimals(a, b), expected);
    check_comparison(a, b, expected);
}

// Compare the integer i with the decimal d from both sides, which must give opposite results.
void check_mixed_comparison(std::int64_t i, double d, int expected)
{
    ensure_eq(compare_traits::compare_integer_decimal(i, d), expected);
    check_comparison(i, d, expected);
    check_comparison(d, i, -expected);
}

// compare must be antisymmetric, and both "less than" and "equivalent to" must be transitive, over every pair and
// triple drawn from numbers.
void ensure_strict_weak_ordering(const std::vector<value>& numbers)
{
    for (const value& a : numbers)
    {
        ensure_eq(compare(a, a), 0);
        ensure(!(a < a));
        for (const value& b : numbers)
        {
            ensure_eq(compare(a, b), -compare(b, a));
            for (const value& c : numbers)
            {
                if (compare(a, b) < 0 && compare(b, c) < 0)
                {
                    ensure_lt(compare(a, c), 0);
                    ensure_lt(a.compare(c), 0);
                    ensure(a < c);
                }
                if (compare(a, b) == 0 && compare(b, c) == 0)
                {
                    ensure_eq(compare(a, c), 0);
                    ensure_eq(a.compare(c), 0);
                    ensure(a == c);
                }
            }
        }
    }
}

// Equivalence classes of numbers in ascending order, mixing kind::integer and kind::decimal. They cluster where
// converting an integer to double loses it: past +/-2^53, where a double stops holding every integer, and at the ends
// of the int64 range, where doubles are 1024 apart and INT64_MAX rounds up to 2^63, which no int64 holds.
std::vector<std::vector<value>> mixed_number_classes()
{
    const double       inf = std::numeric_limits<double>::infinity();
    const double       nan = std::numeric_limits<double>::quiet_NaN();
    const double       d   = std::numeric_limits<double>::denorm_min();
    const std::int64_t min = std::numeric_limits<std::int64_t>::min();
    const std::int64_t max = std::numeric_limits<std::int64_t>::max();
    const std::int64_t i52 = std::int64_t(1) << 52;
    const std::int64_t i53 = std::int64_t(1) << 53;
    return {
        { -inf }, { std::numeric_limits<double>::lowest() }, { std::nextafter(-0x1p63, -inf) },
        { min, -0x1p63 }, { min + 1 }, { min + 1024, std::nextafter(-0x1p63, 0.0) }, { min + 1025 },
        { -i53 - 2, -0x1p53 - 2.0 }, { -i53 - 1 }, { -i53, -0x1p53 }, { -i53 + 1, -0x1p53 + 1.0 },
        { -i52, -0x1p52 }, { -0x1p52 + 0.5 }, { -i52 + 1, -0x1p52 + 1.0 },
        { -2, -2.0 }, { -1.5 }, { -1, -1.0 }, { -0.5 }, { -d }, { 0, 0.0, -0.0 }, { d }, { 0.5 }, { 1, 1.0 }, { 1.5 },
        { 2, 2.0 },
        { i52 - 1, 0x1p52 - 1.0 }, { 0x1p52 - 0.5 }, { i52, 0x1p52 },
        { i53 - 1, 0x1p53 - 1.0 }, { i53, 0x1p53 }, { i53 + 1 }, { i53 + 2, 0x1p53 + 2.0 }, { i53 + 3 },
        { max - 1024 }, { max - 1023, std::nextafter(0x1p63, 0.0) }, { max - 1022 }, { max },
        { 0x1p63 }, { std::nextafter(0x1p63, inf) }, { std::numeric_limits<double>::max() }, { inf },
        { nan, -nan, std::nan("1") }
    };
}

std::vector<value> flatten(const std::vector<std::vector<value>>& classes)
{
    std::vector<value> out;
    for (const std::vector<value>& equivalents : classes)
        out.insert(out.end(), equivalents.begin(), equivalents.end());
    return out;
}

}

TEST(compare_decimals_denormal_transitivity)
{
    const double d = std::numeric_limits<double>::denorm_min();
    const double numbers[] = { -11.0 * d, -5.0 * d, 0.0, 5.0 * d, 11.0 * d };
    for (double a : numbers)
        for (double b : numbers)
            check_decimal_comparison(a, b, a == b ? 0 : a < b ? -1 : 1);
}

TEST(compare_decimals_adjacent)
{
    const double inf = std::numeric_limits<double>::infinity();
    for (double a : { -1.0, 0.0, std::numeric_limits<double>::min(), 1.0 })
    {
        const double below = std::nextafter(a, -inf);
        const double above = std::nextafter(a, inf);
        check_decimal_comparison(below, a, -1);
        check_decimal_comparison(a, below, 1);
        check_decimal_comparison(a, a, 0);
        check_decimal_comparison(a, above, -1);
        check_decimal_comparison(above, a, 1);
    }
}

TEST(compare_decimals_special_values)
{
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // Each group is an equivalence class, listed in ascending order.
    const std::vector<std::vector<double>> groups = {
        { -inf }, { std::numeric_limits<double>::lowest() }, { -1.0 },
        { -0.0, 0.0 }, { 1.0 }, { std::numeric_limits<double>::max() },
        { inf }, { nan, -nan, std::nan("1"), std::nan("2") }
    };
    for (std::size_t i = 0; i < groups.size(); ++i)
        for (std::size_t j = 0; j < groups.size(); ++j)
            for (double a : groups[i])
                for (double b : groups[j])
                    check_decimal_comparison(a, b, i == j ? 0 : i < j ? -1 : 1);
}

TEST(compare_decimals_strict_weak_ordering)
{
    const double d = std::numeric_limits<double>::denorm_min();
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    ensure_strict_weak_ordering({
        -inf, -1.0, -11.0 * d, -5.0 * d, -0.0, 0.0, 5.0 * d, 11.0 * d,
        1.0, std::nextafter(1.0, 2.0), inf, nan, -nan, std::nan("1")
    });
}

TEST(compare_decimals_ordered_containers)
{
    const double d = std::numeric_limits<double>::denorm_min();
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<value> numbers = { nan, 11.0 * d, inf, -0.0, -inf, 5.0 * d, -nan, 0.0 };
    const std::vector<value> expected = { -inf, 0.0, 5.0 * d, 11.0 * d, inf, nan };
    const std::set<value> unique(numbers.begin(), numbers.end());
    ensure_eq(unique.size(), expected.size());
    ensure(std::equal(unique.begin(), unique.end(), expected.begin(), expected.end()));
    std::sort(numbers.begin(), numbers.end());
    numbers.erase(std::unique(numbers.begin(), numbers.end()), numbers.end());
    ensure(numbers == expected);
}

TEST(compare_integer_decimal_past_2_53)
{
    // Converting c to double rounds it down onto b, which used to make a == b and b == c, yet a < c (#199).
    const value a(std::int64_t(1) << 53);
    const value b(0x1p53);
    const value c((std::int64_t(1) << 53) + 1);
    ensure_eq(compare(a, b), 0);
    ensure_lt(compare(b, c), 0);
    ensure_lt(compare(a, c), 0);
    ensure(a == b);
    ensure(b < c);
    ensure(a < c);
}

TEST(compare_integer_decimal_ordering)
{
    const std::vector<std::vector<value>> classes = mixed_number_classes();
    for (std::size_t i = 0; i < classes.size(); ++i)
        for (std::size_t j = 0; j < classes.size(); ++j)
            for (const value& a : classes[i])
                for (const value& b : classes[j])
                {
                    const int expected = i == j ? 0 : i < j ? -1 : 1;
                    if (a.kind() == kind::integer && b.kind() == kind::decimal)
                        check_mixed_comparison(a.as_integer(), b.as_decimal(), expected);
                    else
                        check_comparison(a, b, expected);
                }
}

TEST(compare_integer_decimal_strict_weak_ordering)
{
    ensure_strict_weak_ordering(flatten(mixed_number_classes()));
}

TEST(compare_integer_decimal_ordered_containers)
{
    const std::vector<std::vector<value>> classes = mixed_number_classes();
    std::vector<value> expected;
    for (const std::vector<value>& equivalents : classes)
        expected.push_back(equivalents.front());

    std::vector<value> numbers = flatten(classes);
    std::reverse(numbers.begin(), numbers.end());
    const std::set<value> unique(numbers.begin(), numbers.end());
    ensure_eq(unique.size(), expected.size());
    ensure(std::equal(unique.begin(), unique.end(), expected.begin(), expected.end()));
    std::sort(numbers.begin(), numbers.end());
    numbers.erase(std::unique(numbers.begin(), numbers.end()), numbers.end());
    ensure(numbers == expected);

    // A decimal key finds the one integer it holds exactly, not the neighbor which rounds onto it.
    const std::int64_t i53 = std::int64_t(1) << 53;
    const std::map<value, int> keys = { { i53, 0 }, { i53 + 1, 1 } };
    ensure_eq(keys.size(), 2U);
    ensure_eq(keys.at(0x1p53), 0);
    const auto [first, last] = keys.equal_range(0x1p53);
    ensure_eq(std::distance(first, last), 1);
    ensure(keys.find(0x1p53 + 2.0) == keys.end());
}

TEST(compare_integer_decimal_reverses_custom_extremes)
{
    // Custom traits may answer with any magnitude. Reversing one for the decimal-on-the-left order must not negate it,
    // since INT_MIN has no negation.
    struct extreme_traits : compare_traits
    {
        static int compare_integer_decimal(std::int64_t a, double b)
        {
            const int cmp = compare_traits::compare_integer_decimal(a, b);
            return cmp < 0 ? std::numeric_limits<int>::min()
                 : cmp > 0 ? std::numeric_limits<int>::max()
                 :           0;
        }
    };
    ensure_gt(compare(value(2.0), value(1), extreme_traits()), 0);
    ensure_lt(compare(value(0.0), value(1), extreme_traits()), 0);
    ensure_eq(compare(value(1.0), value(1), extreme_traits()), 0);
}

TEST(compare_integer_decimal_nested)
{
    const std::int64_t i53 = std::int64_t(1) << 53;
    check_comparison(array({ i53 }), array({ 0x1p53 }), 0);
    check_comparison(array({ i53 + 1 }), array({ 0x1p53 }), 1);
    check_comparison(array({ 0x1p53 }), array({ i53 + 1 }), -1);
    check_comparison(object({ { "n", i53 } }), object({ { "n", 0x1p53 } }), 0);
    check_comparison(object({ { "n", i53 + 1 } }), object({ { "n", 0x1p53 } }), 1);
    check_comparison(object({ { "n", 0x1p53 } }), object({ { "n", i53 + 1 } }), -1);

    const std::set<value> arrays = { array({ i53 }), array({ 0x1p53 }), array({ i53 + 1 }) };
    ensure_eq(arrays.size(), 2U);
    ensure_eq(arrays.count(array({ 0x1p53 })), 1U);
}

TEST(compare_icase_sames)
{
    ensure_eq(compare_icase("A", "a"), 0);
    ensure_eq(compare_icase("a", "A"), 0);
    ensure_eq(compare_icase("a", "a"), 0);
    ensure_eq(compare_icase("A", "A"), 0);
}

TEST(compare_icase_diffs)
{
    ensure_lt(compare_icase("A", "b"), 0);
    ensure_lt(compare_icase("a", "B"), 0);
    ensure_gt(compare_icase("b", "a"), 0);
    ensure_gt(compare_icase("B", "A"), 0);
}

TEST(compare_icase_empty)
{
    ensure_eq(compare_icase("",  ""), 0);
    ensure_gt(compare_icase("a", ""), 0);
    ensure_lt(compare_icase("", "a"), 0);
}

}
