/// \file
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/parse.hpp>

#include <algorithm>
#include <memory>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

TEST(array)
{
    jsonv::value arr = jsonv::array();
    arr.push_back(8.9);
    ensure(arr.size() == 1);
    ensure(arr[0].kind() == jsonv::kind::decimal);
    arr.push_back(true);
    ensure(arr.size() == 2);
    ensure(arr[0].kind() == jsonv::kind::decimal);
    ensure(arr[1].kind() == jsonv::kind::boolean);
    arr[0] = "Hi";
    ensure(arr.size() == 2);
    ensure(arr[0].kind() == jsonv::kind::string);
    ensure(arr[1].kind() == jsonv::kind::boolean);
}

TEST(array_init_list)
{
    jsonv::value arr1 = jsonv::array({ 1, 2, "pie" });
    jsonv::value arr2 = jsonv::array();
    {
        arr2.push_back(1);
        arr2.push_back(2);
        arr2.push_back("pie");
    }

    ensure_eq(arr1, arr2);
}

TEST(make_array)
{
    jsonv::value arr = jsonv::array({ 2, 10, "Hello, world!" });
    ensure(arr.kind() == jsonv::kind::array);
    ensure(arr.size() == 3);
    ensure(arr[0].as_integer() == 2);
    ensure(arr[1].as_integer() == 10);
    ensure(arr[2].as_string() == "Hello, world!");
}

TEST(parse_array)
{
    jsonv::value arr = jsonv::parse("\t\n[2, 10, \"Hello, world!\"]   ");
    ensure(arr.kind() == jsonv::kind::array);
    ensure_eq(arr.size(), 3);
    ensure_eq(arr[0].as_integer(), 2);
    ensure_eq(arr[1].as_integer(), 10);
    ensure_eq(arr[2].as_string(), "Hello, world!");
    ensure(arr == jsonv::array({ 2, 10, "Hello, world!" }));
}

TEST(array_view_iter_assign)
{
    using namespace jsonv;

    value val = array({ 0, 1, 2, 3, 4, 5 });
    const value& arr = val;
    int64_t i = 0;
    for (value::const_array_iterator iter = arr.begin_array(); iter != arr.end_array(); ++iter)
    {
        ensure(iter->as_integer() == i);
        ++i;
    }
}

TEST(array_iter_postfix_increment)
{
    using namespace jsonv;

    value arr = array({ 0, 1 });

    auto check = [](auto iter, auto last)
    {
        auto prev = iter++;
        ensure_eq(prev->as_integer(), 0);
        ensure_eq(iter->as_integer(), 1);
        prev = iter++;
        ensure_eq(prev->as_integer(), 1);
        ensure(iter == last);
    };
    check(arr.begin_array(), arr.end_array());

    const value& carr = arr;
    check(carr.begin_array(), carr.end_array());
}

TEST(array_iter_postfix_decrement)
{
    using namespace jsonv;

    value arr = array({ 0, 1 });

    auto check = [](auto iter)
    {
        auto last = iter;
        auto prev = iter--;
        ensure(prev == last);
        ensure_eq(iter->as_integer(), 1);
        prev = iter--;
        ensure_eq(prev->as_integer(), 1);
        ensure_eq(iter->as_integer(), 0);
    };
    check(arr.end_array());

    const value& carr = arr;
    check(carr.end_array());
}

TEST(array_erase_single)
{
    using namespace jsonv;

    value arr = array({ 0, 1, 2, 3, 4, 5 });
    ensure_eq(arr.size(), 6);
    auto iter = arr.erase(arr.begin_array() + 2);
    ensure_eq(iter->as_integer(), 3);
    ensure_eq(arr.size(), 5);
    ensure_eq(arr, array({ 0, 1, 3, 4, 5 }));
}

TEST(array_erase_multi)
{
    using namespace jsonv;

    value arr = array({ 0, 1, 2, 3, 4, 5 });
    ensure_eq(arr.size(), 6);
    auto iter = arr.erase(arr.begin_array() + 2, arr.begin_array() + 4);
    ensure_eq(iter->as_integer(), 4);
    ensure_eq(arr.size(), 4);
    ensure_eq(arr, array({ 0, 1, 4, 5 }));
}

TEST(array_erase_multi_to_end)
{
    using namespace jsonv;

    value arr = array({ 0, 1, 2, 3, 4, 5 });
    ensure_eq(arr.size(), 6);
    auto iter = arr.erase(arr.begin_array() + 3, arr.end_array());
    ensure(iter == arr.end_array());
    ensure_eq(arr.size(), 3);
    ensure_eq(arr, array({ 0, 1, 2 }));
}

TEST(array_erase_multi_from_begin)
{
    using namespace jsonv;

    value arr = array({ 0, 1, 2, 3, 4, 5 });
    ensure_eq(arr.size(), 6);
    auto iter = arr.erase(arr.begin_array(), arr.end_array() - 3);
    ensure(iter == arr.begin_array());
    ensure_eq(iter->as_integer(), 3);
    ensure_eq(arr.size(), 3);
    ensure_eq(arr, array({ 3, 4, 5 }));
}

TEST(array_push_move)
{
    jsonv::value arr = jsonv::array();
    jsonv::value val = "contents";
    arr.push_back(std::move(val));
    ensure(val.kind() == jsonv::kind::null);
}

TEST(array_algo_sort)
{
    using namespace jsonv;

    value arr = array({ 9, 1, 3, 4, 2, 8, 6, 7, 0, 5 });
    ensure_eq(arr.size(), 10);
    std::sort(arr.begin_array(), arr.end_array());
    ensure_eq(arr, array({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }));
}

TEST(array_view)
{
    using namespace jsonv;

    const value arr1 = array({ "pie", 5, null, 0 });
    value arr2 = array();
    for (const value& val : arr1.as_array())
        arr2.push_back(val);

    ensure_eq(arr1, arr2);
}

TEST(array_push_pop)
{
    using namespace jsonv;

    value arr = array({ 1, 2, 3 });
    ensure(!arr.empty());
    arr.insert(arr.begin_array(), 0);
    {
        std::int64_t exp = 0;
        for (const value& x : arr.as_array())
        {
            ensure_eq(exp, x.as_integer());
            ensure_eq(x, arr.at(exp));
            ++exp;
        }
    }
    ensure_throws(std::out_of_range, arr.at(10));
    ensure_eq(4U, arr.size());
    arr.pop_back();
    ensure_eq(3U, arr.size());
    arr.pop_back();
    ensure_eq(2U, arr.size());
    arr.assign(5, null);
    ensure_eq(array({ null, null, null, null, null }), arr);
}

TEST(array_insertion)
{
    using namespace jsonv;

    value arr = array({ 1, 2, 3, 4 });
    auto iter = arr.insert(arr.begin_array(), 0);
    ensure(arr.begin_array() == iter);
    ensure_eq(arr[0], 0);
    ensure_eq(arr, array({ 0, 1, 2, 3, 4 }));
    iter = arr.insert(arr.end_array(), 5);
    ensure((arr.end_array() - 1) == iter);
    ensure_eq(arr[5], 5);
    ensure_eq(arr, array({ 0, 1, 2, 3, 4, 5 }));
}

TEST(array_multi_insertion)
{
    using namespace jsonv;

    value arr = array({ 0, 1, 4, 5 });
    std::vector<std::size_t> to_add({ 2, 3 });
    auto iter = arr.insert(arr.begin_array() + 2, to_add.begin(), to_add.end());
    ensure((arr.begin_array() + 2) == iter);
    ensure_eq(arr, array({ 0, 1, 2, 3, 4, 5 }));
}

TEST(array_iterate_over_temp)
{
    using namespace jsonv;
    std::ostringstream os;
    for (const value& x : array({ 1, "two", "three", array({ 4, 4, 4 }) }).as_array())
        os << x;
}

namespace
{

using owning_array_view = jsonv::value::owning_array_view;

// The elements of a view, gathered into an array so a failure prints them.
template <typename TView>
jsonv::value elements_of(TView& view)
{
    jsonv::value out = jsonv::array();
    for (const jsonv::value& x : view)
        out.push_back(x);
    return out;
}

// A by-value parameter is never a candidate for NRVO, so returning it makes the view move a second time.
owning_array_view pass_through(owning_array_view view)
{
    return view;
}

}

// A view owns its container, so it cannot be copied; copying would leave the iterators on the source.
static_assert(!std::is_copy_constructible<owning_array_view>::value, "an owning view must not be copyable");
static_assert(!std::is_copy_assignable<owning_array_view>::value,    "an owning view must not be copyable");
static_assert(std::is_nothrow_move_constructible<owning_array_view>::value, "an owning view must be movable");
static_assert(std::is_nothrow_move_assignable<owning_array_view>::value,    "an owning view must be movable");

TEST(array_owning_view_move_construct)
{
    using namespace jsonv;

    const value original = array({ 1, 2, 3 });
    value copy = original;
    auto from = std::move(copy).as_array();
    owning_array_view to(std::move(from));

    ensure_eq(array({ 1, 2, 3 }), elements_of(to));

    // The destination owns its own container, so a write through it reaches nothing else.
    *to.begin() = 9;
    ensure_eq(array({ 9, 2, 3 }), elements_of(to));
    ensure_eq(array({ 1, 2, 3 }), original);
}

TEST(array_owning_view_move_construct_then_destroy_source)
{
    using namespace jsonv;

    auto from = std::make_unique<owning_array_view>(array({ 1, 2, 3 }).as_array());
    owning_array_view to(std::move(*from));
    from.reset();

    ensure_eq(array({ 1, 2, 3 }), elements_of(to));
    *(to.begin() + 1) = 8;
    ensure_eq(array({ 1, 8, 3 }), elements_of(to));
}

TEST(array_owning_view_move_assign)
{
    using namespace jsonv;

    auto from = std::make_unique<owning_array_view>(array({ 1, 2, 3 }).as_array());
    auto to = array({ "old" }).as_array();
    to = std::move(*from);
    from.reset();

    ensure_eq(array({ 1, 2, 3 }), elements_of(to));
    *to.begin() = 7;
    ensure_eq(array({ 7, 2, 3 }), elements_of(to));
}

TEST(array_owning_view_move_through_function)
{
    using namespace jsonv;

    auto view = pass_through(array({ 1, 2, 3 }).as_array());
    ensure_eq(array({ 1, 2, 3 }), elements_of(view));
}

TEST(array_owning_view_move_reverse_iteration)
{
    using namespace jsonv;

    auto from = std::make_unique<owning_array_view>(array({ 1, 2, 3 }).as_array());
    owning_array_view to(std::move(*from));
    from.reset();

    auto riter = to.rbegin();
    ensure_eq(*riter, 3);
    ++riter;
    ensure_eq(*riter, 2);
    ++riter;
    ensure_eq(*riter, 1);
    ++riter;
    ensure(riter == to.rend());
}

// The const members which end a reverse walk built a reverse_iterator from the const_iterator that begin() returns, so
// none of them compiled for a view whose iterator and const_iterator are different types.
TEST(array_view_const_reverse_iteration)
{
    using namespace jsonv;

    auto check = [](auto riter, auto rlast)
    {
        ensure_eq(*riter, 3);
        ++riter;
        ensure_eq(*riter, 2);
        ++riter;
        ensure_eq(*riter, 1);
        ++riter;
        ensure(riter == rlast);
    };

    value arr = array({ 1, 2, 3 });
    const auto view = arr.as_array();
    static_assert(std::is_same_v<decltype(view.rend()),  value::array_view::const_reverse_iterator>);
    static_assert(std::is_same_v<decltype(view.crend()), value::array_view::const_reverse_iterator>);
    check(view.rbegin(),  view.rend());
    check(view.crbegin(), view.crend());

    // crend() is const no matter what it is called on, so a view which is not const could not call it either.
    auto mutable_view = arr.as_array();
    check(mutable_view.crbegin(), mutable_view.crend());

    const owning_array_view owned = array({ 1, 2, 3 }).as_array();
    check(owned.rbegin(),  owned.rend());
    check(owned.crbegin(), owned.crend());
}

TEST(array_owning_view_moved_from_is_empty)
{
    using namespace jsonv;

    auto from = array({ 1, 2, 3 }).as_array();
    owning_array_view to(std::move(from));

    ensure(from.begin() == from.end());
    ensure(from.rbegin() == from.rend());
    ensure_eq(array(), elements_of(from));

    // Moving from a view which has already been moved from must not try to look inside the empty value.
    owning_array_view again(std::move(from));
    ensure(again.begin() == again.end());
    to = std::move(again);
    ensure(to.begin() == to.end());
}

TEST(array_owning_view_self_move_assign)
{
    using namespace jsonv;

    auto view = array({ 1, 2, 3 }).as_array();
    owning_array_view& alias = view;
    view = std::move(alias);

    ensure_eq(array({ 1, 2, 3 }), elements_of(view));
}

TEST(parse_empty_array)
{
    auto arr = jsonv::parse("[]");

    ensure_eq(arr.size(), 0);
    ensure(arr.empty());
}
