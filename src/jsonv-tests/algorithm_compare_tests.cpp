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
#include <limits>
#include <set>
#include <vector>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

void check_decimal_comparison(double a, double b, int expected)
{
    // Use distinct value objects so the same-address shortcut cannot hide a regression.
    const value left(a);
    const value right(b);
    ensure_eq(compare_traits::compare_decimals(a, b), expected);
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
    const std::vector<value> numbers = {
        -inf, -1.0, -11.0 * d, -5.0 * d, -0.0, 0.0, 5.0 * d, 11.0 * d,
        1.0, std::nextafter(1.0, 2.0), inf, nan, -nan, std::nan("1")
    };
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
