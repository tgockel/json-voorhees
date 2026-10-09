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
#include "allocation_counter.hpp"

#include <jsonv/all.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <string>
#include <string_view>
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

#if JSONV_TEST_COUNTS_ALLOCATIONS

namespace
{

/// The allocations building a `value` from \a text costs, in whichever form the constructor takes it.
template <typename TText>
std::size_t string_construction_cost(const TText& text)
{
    jsonv_test::allocation_counter allocations;
    jsonv::value                   built(text);
    return allocations.count();
}

/// Builds a `value` from \a text, failing each allocation of at least \a min_size bytes that takes in turn until an
/// attempt goes through, and returns how many attempts failed. Every attempt, failed or not, has to give back
/// everything it allocated.
template <typename TText>
std::size_t string_construction_failures(const TText& text, std::size_t min_size)
{
    for (std::size_t nth = 1U; ; ++nth)
    {
        const std::size_t before = jsonv_test::live_allocations();
        bool              failed = false;
        {
            // Nothing but the constructor may allocate in here. That rules out `ensure_throws`, whose failure path
            // allocates and so can catch the `std::bad_alloc` it caused itself and pass.
            jsonv_test::failing_allocation fail(nth, min_size);
            try
            {
                jsonv::value built(text);
            }
            catch (const std::bad_alloc&)
            {
                failed = true;
            }
        }
        const std::size_t after = jsonv_test::live_allocations();
        ensure_eq(before, after);

        if (!failed)
            return nth - 1U;
    }
}

}

/// Building a `value` from a string copies the text once, whichever form it arrives in. Copying text past the
/// small-string buffer is an allocation and copying text inside it is not, while whatever else a `std::string` costs
/// -- under MSVC's debug iterator checks, constructing or moving one allocates an iterator proxy -- it costs whatever
/// the length. So the number of copies is the difference between building from a long string and from a short one,
/// and a second copy would show as a difference of two.
TEST(value_string_constructors_copy_once)
{
    const std::string long_text(64, 'x');
    const std::string short_text(1, 'x');

    ensure_eq(string_construction_cost(short_text) + 1U, string_construction_cost(long_text));
    ensure_eq(string_construction_cost(std::string_view(short_text)) + 1U,
              string_construction_cost(std::string_view(long_text)));
    ensure_eq(string_construction_cost(short_text.c_str()) + 1U, string_construction_cost(long_text.c_str()));
}

/// Building a `value` from a string allocates the node before the text is copied or converted into it, so a copy or a
/// conversion which throws has to give the node back -- the constructor never finishes, so `~value` never runs to do
/// it. The node used to be allocated and the text assigned into it afterwards, which leaked the node when the copy
/// threw (#272).
///
/// Only allocations at least as long as the text fail, which are the buffers it is copied or converted into. Anything
/// smaller may be bookkeeping a failure cannot propagate from: MSVC's debug containers allocate an iterator proxy inside
/// `noexcept` constructors and moves. The `std::range_error` a refused wide string throws from the same place is left
/// to `wide_strings_wider_than_utf16` and the sanitizer build's leak check, since `live_allocations` cannot see that
/// exception's message freed under libc++abi 18.
TEST(value_string_constructors_leak_nothing_when_the_copy_fails)
{
    const std::size_t  length = 256U;
    const std::string  text(length, 'x');
    const std::wstring wide(length, L'x');

    // The text is copied or converted into a buffer of at least its length, so a constructor which went through first
    // time never had the copy fail.
    ensure_le(1U, string_construction_failures(text, length));
    ensure_le(1U, string_construction_failures(std::string_view(text), length));
    ensure_le(1U, string_construction_failures(text.c_str(), length));
    ensure_le(1U, string_construction_failures(wide, length));
    ensure_le(1U, string_construction_failures(wide.c_str(), length));
}

#endif
