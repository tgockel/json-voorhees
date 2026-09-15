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
