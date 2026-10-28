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

namespace
{

/// \a depth arrays nested one inside the next, the innermost empty. Built by moving each level into the next rather
/// than with `jsonv::array({ ... })`, whose `initializer_list` elements are `const` and so would copy.
jsonv::value deeply_nested_arrays(std::size_t depth)
{
    jsonv::value nested = jsonv::array();
    for (std::size_t level = 1U; level < depth; ++level)
    {
        jsonv::value outer = jsonv::array();
        outer.push_back(std::move(nested));
        nested = std::move(outer);
    }
    return nested;
}

/// \a depth objects nested one inside the next under \a key, each also holding a scalar and a string long enough to
/// be stored out of line, so the teardown has elements beside the nesting to free at every level.
jsonv::value deeply_nested_objects(std::size_t depth, const std::string& key)
{
    jsonv::value nested = jsonv::object();
    for (std::size_t level = 1U; level < depth; ++level)
    {
        jsonv::value outer = jsonv::object();
        outer["number"]    = std::int64_t(level);
        outer["text"]      = std::string(64U, 'x');
        outer[key]         = std::move(nested);
        nested             = std::move(outer);
    }
    return nested;
}

/// A chain of \a depth arrays, each also holding \a width arrays of its own, each of those holding a string kept out
/// of line.
///
/// A plain chain only ever leaves one container waiting to be freed, however deep it is: the teardown recurses to its
/// budget, sets the single child past it aside, and comes straight back for it. Fanning out at every level means that
/// whichever level is the one to run out of budget has \a width containers to set aside at once, so the room for them
/// has to grow -- and grow again -- without the test needing to know where that level falls.
jsonv::value deep_and_wide(std::size_t depth, std::size_t width)
{
    jsonv::value nested = jsonv::array();
    for (std::size_t level = 1U; level < depth; ++level)
    {
        jsonv::value outer = jsonv::array();
        for (std::size_t idx = 0U; idx < width; ++idx)
            outer.push_back(jsonv::array({ std::string(64U, 'x') }));
        outer.push_back(std::move(nested));
        nested = std::move(outer);
    }
    return nested;
}

/// How deep the trees the destruction tests build are. Destroying one used to recurse once per level, so this has to
/// be past what a thread's stack can hold those frames for -- it was a few tens of thousands of levels on the usual
/// 8MB stack, and the sanitizer builds use far more stack per frame than that. It also has to stay cheap enough to
/// build a few times over, since each level is an allocation.
constexpr std::size_t destruction_depth = 1000U * 1000U;

}

/// Destroying a deeply nested tree must not recurse once per level: the stack it would take is unbounded, and `~value`
/// is `noexcept`, so overflowing it kills the process where no caller can do anything about it. Nothing bounds how
/// deep a tree built through the mutators is -- a parse is bounded by `parse_options::k::max_structure_depth`, but
/// `push_back` and `path` are not (#416).
TEST(value_destroy_deeply_nested_arrays)
{
    (void) deeply_nested_arrays(destruction_depth);
}

/// The same for objects, whose members are freed through a different block type, and with something other than the
/// nesting at each level to be sure the rest of a level is still freed.
TEST(value_destroy_deeply_nested_objects)
{
    (void) deeply_nested_objects(destruction_depth, "nested");
}

/// A key long enough to be stored out of line is freed by the same walk, so what it allocated has to come back too.
/// The sanitizer builds' leak check is what sees this.
TEST(value_destroy_deeply_nested_objects_with_long_keys)
{
    (void) deeply_nested_objects(destruction_depth, std::string(64U, 'k'));
}

/// `clear` is the same teardown as the destructor, and leaves a null which is good for anything afterwards.
TEST(value_clear_deeply_nested)
{
    jsonv::value deep = deeply_nested_arrays(destruction_depth);
    deep.clear();

    ensure(deep.is_null());
    ensure_eq(jsonv::kind::null, deep.kind());

    deep = jsonv::array({ 1, 2, 3 });
    ensure_eq(std::size_t(3), deep.size());
}

/// Assigning over a deeply nested tree destroys it, by either form of assignment.
TEST(value_assign_over_deeply_nested)
{
    jsonv::value deep = deeply_nested_arrays(destruction_depth);
    deep              = jsonv::value(5);
    ensure_eq(std::int64_t(5), deep.as_integer());

    const jsonv::value small = jsonv::array({ 1 });

    deep = deeply_nested_arrays(destruction_depth);
    deep = small;
    ensure_eq(small, deep);
}

/// Moving a descendant over the value holding it has to work: the source lives inside the block the destination owns,
/// so clearing the destination first frees the source out from under the read which follows. It used to clear first,
/// which read freed storage and left the destination owning whatever came back.
TEST(value_move_assign_descendant_over_its_parent)
{
    const jsonv::value expected = jsonv::object({ { "inner", std::string(64U, 'x') } });

    jsonv::value doc = jsonv::object({ { "data", expected } });
    doc              = std::move(doc["data"]);
    ensure_eq(expected, doc);

    // An element of an array, and a scalar rather than a container.
    jsonv::value arr = jsonv::array({ jsonv::array({ 1, 2, 3 }) });
    arr              = std::move(arr.at(0));
    ensure_eq(jsonv::array({ 1, 2, 3 }), arr);

    jsonv::value holder = jsonv::object({ { "n", 5 } });
    holder              = std::move(holder["n"]);
    ensure_eq(jsonv::value(5), holder);

    // A string kept out of line: the destination would otherwise own text which has already been freed.
    jsonv::value text = jsonv::object({ { "s", std::string(64U, 'y') } });
    text              = std::move(text["s"]);
    ensure_eq(jsonv::value(std::string(64U, 'y')), text);

    // A descendant further down than a direct child.
    jsonv::value deep = jsonv::object({ { "a", jsonv::object({ { "b", expected } }) } });
    deep              = std::move(deep["a"]["b"]);
    ensure_eq(expected, deep);
}

/// The same for copy assignment, which goes through a copy of the source before the destination is touched.
TEST(value_copy_assign_descendant_over_its_parent)
{
    const jsonv::value expected = jsonv::object({ { "inner", std::string(64U, 'x') } });

    jsonv::value doc = jsonv::object({ { "data", expected } });
    doc              = doc["data"];
    ensure_eq(expected, doc);
}

/// What a deeply nested tree holds beside the nesting must still be freed: a level whose elements were dropped instead
/// of destroyed would leak everything hanging off them, which the sanitizer builds' leak check sees. Each level here
/// also holds text too long for the small-string buffer, so each is an allocation of its own.
TEST(value_destroy_deeply_nested_frees_the_elements_beside_the_nesting)
{
    constexpr std::size_t depth = 1000U;

    jsonv::value nested = jsonv::array();
    for (std::size_t level = 1U; level < depth; ++level)
    {
        jsonv::value outer = jsonv::array();
        outer.push_back(std::string(64U, 'x'));
        outer.push_back(jsonv::object({ { "key-longer-than-the-small-string-buffer", std::string(64U, 'y') } }));
        outer.push_back(std::move(nested));
        nested = std::move(outer);
    }
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

/// A tree within the depth a parse could have produced is torn down from the call stack, so nothing is allocated to
/// destroy it. The iterative teardown keeps the levels past that on the heap, and the point of the budget is that the
/// shallow trees which are the usual case never pay for one.
TEST(value_destroy_shallow_tree_allocates_nothing)
{
    // `max_structure_depth` is 128; this stays well inside it while still nesting.
    jsonv::value shallow = deeply_nested_arrays(100U);

    // Every allocation is counted, down to the 16-byte iterator proxy MSVC's debug containers make: the teardown
    // holds no container of its own, so a shallow one must ask for nothing whatsoever.
    jsonv_test::allocation_counter allocations;
    shallow.clear();
    const std::size_t cost = allocations.count();
    ensure_eq(std::size_t(0), cost);
}

/// Destroying a shallow value must not even be able to fail. The teardown holds no container of its own, so there is
/// no allocation on this path to go wrong -- with the first allocation of any size armed to fail, clearing an empty
/// array, an empty object and a shallow tree still goes through and still gives everything back. A `std::vector` in
/// the teardown would allocate its iterator proxy here under MSVC's debug checks, inside a `noexcept` constructor,
/// and a refusal there aborts the process rather than being catchable.
TEST(value_destroy_shallow_tree_cannot_fail_when_allocation_does)
{
    jsonv::value values[] = { jsonv::array(),
                              jsonv::object(),
                              jsonv::array({ 1, std::string(64U, 'x') }),
                              deeply_nested_arrays(100U),
                            };

    for (jsonv::value& shallow : values)
    {
        const std::size_t before = jsonv_test::live_allocations();
        {
            jsonv_test::failing_allocation fail;
            shallow.clear();
        }
        const std::size_t after = jsonv_test::live_allocations();

        ensure(shallow.is_null());
        ensure_le(after, before);
    }
}

/// The teardown keeps the levels past its recursion budget on the heap, and there is nowhere to report a failure to
/// grow that list from: `~value` is `noexcept`. It falls back to recursing for the subtree it could not remember, so
/// a teardown which cannot allocate still frees everything -- which is what `live_allocations` checks here, along
/// with the sanitizer builds' leak check.
///
/// Getting that room may not throw or terminate either, which is why the teardown does the bookkeeping by hand: a
/// `std::vector` allocates inside a `noexcept` constructor under MSVC's debug iterator checks, so merely having one
/// would make destroying even an empty array able to abort the process when memory is short.
TEST(value_destroy_deeply_nested_when_the_teardown_cannot_allocate)
{
    // Past the recursion budget, and then wide enough that the room for the levels set aside has to be grown several
    // times over, so failing the second and the third request fails a regrowth rather than one which never happens.
    constexpr std::size_t depth = 200U;
    constexpr std::size_t width = 1200U;

    // The teardown asks for room for the levels past its recursion budget, and asks again when that fills up. Each
    // `nth` fails a different one of those requests -- the first, which is the only one a shallow tree reaches, and
    // the regrowths after it -- so the fallback is exercised both before anything is stacked up and part of the way
    // through. Nothing else in here allocates, so the count is the teardown's own.
    for (std::size_t nth = 1U; nth <= 3U; ++nth)
    {
        const std::size_t before = jsonv_test::live_allocations();
        {
            jsonv::value deep = deep_and_wide(depth, width);

            jsonv_test::failing_allocation fail(nth);
            deep.clear();

            ensure(deep.is_null());
        }
        const std::size_t after = jsonv_test::live_allocations();
        ensure_eq(before, after);
    }
}

/// Every block a deeply nested tree holds comes back, however the teardown went about freeing it.
TEST(value_destroy_deeply_nested_frees_everything)
{
    constexpr std::size_t depth = 1000U;

    const std::size_t before = jsonv_test::live_allocations();
    {
        jsonv::value deep  = deeply_nested_objects(depth, std::string(64U, 'k'));
        deep["also"]       = deeply_nested_arrays(depth);
    }
    const std::size_t after = jsonv_test::live_allocations();
    ensure_eq(before, after);
}

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
