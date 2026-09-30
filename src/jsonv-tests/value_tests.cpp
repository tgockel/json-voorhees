/** \file
 *  
 *  Copyright (c) 2012-2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"

#include <jsonv/all.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

TEST(move_to_self)
{
    const jsonv::value orig = jsonv::object({ {"a", 5} });
    jsonv::value x(orig);
    ensure_eq(orig, x);
    jsonv::value& same = x;
    x = std::move(same);
    ensure_eq(orig, x);
}

TEST(compare_bools)
{
    jsonv::value t1(true),
                 t2(true),
                 f1(false),
                 f2(false);
    
    ensure_eq(t1.compare(t1),  0);
    ensure_eq(t1.compare(t2),  0);
    ensure_eq(f1.compare(f1),  0);
    ensure_eq(f1.compare(f2),  0);
    ensure_eq(t1.compare(f1),  1);
    ensure_eq(f1.compare(t2), -1);
    
    ensure(t1 <= t2);
    ensure(t1 >= t2);
    ensure(t1 >  f1);
    ensure(t1 >= f1);
}

TEST(compare_arrs)
{
    jsonv::value a123  = jsonv::array({ 1, 2, 3 }),
                 a1234 = jsonv::array({ 1, 2, 3, 4 }),
                 b1234 = jsonv::array({ 1, 2, 3, 4 });
    
    ensure_eq(a1234.compare(b1234), 0);
    ensure_eq(a123.compare(a1234), -1);
    ensure_eq(a1234.compare(a123),  1);
}

TEST(value_equal_integer_decimal)
{
    ensure_eq(jsonv::value(2), jsonv::value(2.0));
    ensure_eq(jsonv::value(2.0), jsonv::value(2));
}

TEST(value_equal_float_decimal)
{
    ensure_eq(jsonv::value(2.5f), jsonv::value(2.5));
}

TEST(value_store_unordered_map)
{
    std::unordered_map<jsonv::value, std::int64_t> m;
    for (std::int64_t x = 0L; x < 1000; ++x)
    {
        auto ret = m.insert({ x, x });
        ensure(ret.second);
    }
    
    ensure_lt(1, m.bucket_count());
}

TEST(value_decimal_denorm_min_compares)
{
    const jsonv::value x = 0.0;
    const jsonv::value y = std::numeric_limits<double>::denorm_min();

    ensure_ne(x.as_decimal(), y.as_decimal());
    ensure_ne(x, y);
    ensure_lt(x, y);
    ensure_eq(-1, x.compare(y));
    ensure_eq(1, y.compare(x));
}

TEST(swap)
{
    jsonv::value x = jsonv::array({ 1, 2, 3 });
    jsonv::value y = "SOMETHING";
    swap(x, y);
    ensure_eq(jsonv::value("SOMETHING"), x);
    ensure_eq(jsonv::array({ 1, 2, 3 }), y);
}

TEST(swap_same)
{
    jsonv::value x = jsonv::array({ 1, 2, 3 });
    swap(x, x);
    ensure_eq(jsonv::array({ 1, 2, 3 }), x);
}

TEST(is_operations)
{
    jsonv::value num = 2.9;
    jsonv::value in_ = 5;
    jsonv::value arr = jsonv::array({ 1, 2, 3 });
    jsonv::value obj = jsonv::object({ {"arr", arr } });
    jsonv::value str = "SOMETHING";
    jsonv::value bol = true;
    jsonv::value nul = jsonv::null;
    
    ensure(num.is_decimal());
    ensure(in_.is_integer());
    ensure(in_.is_decimal());
    ensure(arr.is_array());
    ensure(obj.is_object());
    ensure(str.is_string());
    ensure(bol.is_boolean());
    ensure(nul.is_null());
}

TEST(hash_set_operations)
{
    jsonv::value num = 2.9;
    jsonv::value arr = jsonv::array({ 1, 2, 3 });
    jsonv::value obj = jsonv::object({ {"arr", arr } });
    jsonv::value str = "SOMETHING";
    jsonv::value bol = true;
    jsonv::value nul = jsonv::null;
    
    std::unordered_set<jsonv::value> set = { num, arr, obj, str, bol, nul };
    ensure_eq(6U, set.size());
    ensure_eq(1U, set.count(arr));
    set.erase(str);
    ensure_eq(0U, set.count(str));
    ensure_eq(5U, set.size());
}

TEST(value_decimal_nan_hashes_equal)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const jsonv::value variants[] = { nan, -nan, std::nan("1"), std::nan("2") };
    const std::size_t expected = std::hash<jsonv::value>()(variants[0]);
    for (const jsonv::value& v : variants)
        ensure_eq(expected, std::hash<jsonv::value>()(v));
}

TEST(value_decimal_nan_unordered_set_lookup)
{
    std::unordered_set<jsonv::value> set;
    set.insert(jsonv::value(std::nan("1")));

    ensure_eq(1U, set.count(jsonv::value(std::nan("2"))));
    ensure_eq(1U, set.count(jsonv::value(-std::numeric_limits<double>::quiet_NaN())));
}

TEST(value_decimal_nan_unordered_set_dedup)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::unordered_set<jsonv::value> set = { nan, -nan, std::nan("1"), std::nan("2") };

    ensure_eq(1U, set.size());
}

TEST(value_decimal_nan_nested_containers)
{
    const jsonv::value arr_a = jsonv::array({ 1, std::nan("1") });
    const jsonv::value arr_b = jsonv::array({ 1, std::nan("2") });
    ensure_eq(std::hash<jsonv::value>()(arr_a), std::hash<jsonv::value>()(arr_b));

    std::unordered_set<jsonv::value> arr_set = { arr_a, arr_b };
    ensure_eq(1U, arr_set.size());

    const jsonv::value obj_a = jsonv::object({ { "x", std::nan("1") } });
    const jsonv::value obj_b = jsonv::object({ { "x", std::nan("2") } });
    ensure_eq(std::hash<jsonv::value>()(obj_a), std::hash<jsonv::value>()(obj_b));

    std::unordered_map<jsonv::value, int> obj_map;
    obj_map.insert({ obj_a, 1 });
    ensure_eq(1U, obj_map.count(obj_b));
}

namespace
{

std::size_t hash_of(const jsonv::value& x)
{
    return std::hash<jsonv::value>()(x);
}

// If a and b compare equal, they must hash equal -- on their own, as the element of an array, and as the value of an
// object member.
void ensure_hash_follows_equality(const jsonv::value& a, const jsonv::value& b)
{
    const std::pair<jsonv::value, jsonv::value> spellings[] = {
        { a, b },
        { jsonv::array({ a }), jsonv::array({ b }) },
        { jsonv::object({ { "n", a } }), jsonv::object({ { "n", b } }) },
    };
    for (const auto& [x, y] : spellings)
        if (x == y)
            ensure_eq(hash_of(x), hash_of(y));
}

// Pairs which compare equal, each spelling a number as a kind::integer on one side and a kind::decimal on the other.
std::vector<std::pair<jsonv::value, jsonv::value>> mixed_numeric_pairs()
{
    return {
        { 2, 2.0 },
        { -2.0, -2 },
        { 0, -0.0 },
        { jsonv::array({ 1, 2 }), jsonv::array({ 1.0, 2.0 }) },
        { jsonv::object({ { "n", 2 } }), jsonv::object({ { "n", 2.0 } }) },
    };
}

}

TEST(value_integer_decimal_hashes_equal)
{
    // Each of these is exact in binary64, so the integer and the decimal are the same number.
    const std::int64_t exact = std::int64_t(1) << 53;
    const std::int64_t integers[] = { 1, 2, -2, 1000000007, exact - 1, exact, -exact,
                                      std::numeric_limits<std::int64_t>::min() };
    for (std::int64_t i : integers)
    {
        const jsonv::value integer(i);
        const jsonv::value decimal(static_cast<double>(i));
        ensure_eq(integer, decimal);
        ensure_hash_follows_equality(integer, decimal);
    }
}

TEST(value_integer_decimal_zero_hashes_equal)
{
    const jsonv::value zeros[] = { 0, 0.0, -0.0 };
    for (const jsonv::value& a : zeros)
        for (const jsonv::value& b : zeros)
        {
            ensure_eq(a, b);
            ensure_hash_follows_equality(a, b);
        }
}

TEST(value_integer_decimal_hash_exact_boundary)
{
    // Above 2^53 a double cannot hold every integer, and INT64_MAX rounds up to 2^63, which no int64 can hold. An
    // integer out here equals only a decimal holding exactly that integer, so INT64_MAX equals no decimal at all
    // (#199). The pairs which do compare equal must hash equal.
    const double inf = std::numeric_limits<double>::infinity();
    const std::int64_t exact = std::int64_t(1) << 53;
    const std::int64_t integers[] = { exact - 1, exact, exact + 1, exact + 2, -exact - 1,
                                      std::numeric_limits<std::int64_t>::max(),
                                      std::numeric_limits<std::int64_t>::min() };
    for (std::int64_t i : integers)
    {
        const double d = static_cast<double>(i);
        for (double near : { std::nextafter(d, -inf), d, std::nextafter(d, inf) })
            ensure_hash_follows_equality(jsonv::value(i), jsonv::value(near));
    }
}

TEST(value_integer_decimal_unordered_set)
{
    for (const auto& [a, b] : mixed_numeric_pairs())
    {
        std::unordered_set<jsonv::value> set = { a };
        ensure(set.find(b) != set.end());
        ensure_eq(1U, set.count(b));
        ensure(!set.insert(b).second);
        ensure_eq(1U, set.size());
        ensure_eq(1U, set.erase(b));
        ensure(set.empty());
    }
}

TEST(value_integer_decimal_unordered_map)
{
    for (const auto& [a, b] : mixed_numeric_pairs())
    {
        std::unordered_map<jsonv::value, int> map;
        map[a] = 1;
        map[b] = 2;
        ensure_eq(1U, map.size());
        ensure_eq(2, map.at(a));
    }
}
