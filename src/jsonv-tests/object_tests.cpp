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
#include "allocation_counter.hpp"

#include <jsonv/encode.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/writer.hpp>

#include <cstdint>
#include <cwchar>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Dereferencing an object iterator gives a proxy, which the iterators still model the standard concepts with.
static_assert(std::random_access_iterator<jsonv::value::object_iterator>);
static_assert(std::random_access_iterator<jsonv::value::const_object_iterator>);
static_assert(std::ranges::random_access_range<jsonv::value::object_view>);
static_assert(std::ranges::random_access_range<jsonv::value::const_object_view>);

// The proxy does not bind to a reference to the owning pair, which would copy the member: in the loop
// `for (const value::object_value_type& m : obj.as_object())`, every iteration.
static_assert(!std::is_convertible_v<jsonv::value::const_object_reference, const jsonv::value::object_value_type&>);
static_assert(!std::is_convertible_v<jsonv::value::object_reference, const jsonv::value::object_value_type&>);
static_assert(std::is_constructible_v<jsonv::value::object_value_type, jsonv::value::const_object_reference>);

// The key can not be written through the proxy, which would break the order members are kept in.
static_assert(std::is_same_v<decltype((std::declval<jsonv::value::object_reference>().first)), const std::string&>);
static_assert(std::is_same_v<decltype((std::declval<jsonv::value::object_reference>().second)), jsonv::value&>);
static_assert(std::is_same_v<decltype((std::declval<jsonv::value::const_object_reference>().second)),
                             const jsonv::value&
                            >
             );

TEST(object)
{
    jsonv::value obj = jsonv::object();
    obj["hi"] = false;
    ensure(obj["hi"].as_boolean() == false);
    obj["yay"] = jsonv::array({ "Hello", "to", "the", "world" });
    ensure(obj["hi"].as_boolean() == false);
    ensure(obj["yay"].size() == 4);
    ensure(obj.size() == 2);
}

TEST(object_view_iter_assign)
{
    using namespace jsonv;

    value obj = object({ { "foo", 5 }, { "bar", "wat" } });
    value found = object({ { "foo", false }, { "bar", false } });
    ensure(obj.size() == 2);

    for (auto iter = obj.begin_object(); iter != obj.end_object(); ++iter)
    {
        value::object_iterator fiter;
        fiter = found.find(iter->first);
        ensure(!fiter->second.as_boolean());
        fiter->second = true;
    }

    for (auto iter = found.begin_object(); iter != found.end_object(); ++iter)
        ensure(iter->second.as_boolean());
}

TEST(object_view_reverse_iter)
{
    using namespace jsonv;

    value obj = object({ { "a", 1 }, { "b", 2 }, { "c", 3 } });
    auto riter = obj.as_object().rbegin();
    ensure_eq(riter->first, "c");
    ++riter;
    ensure_eq(riter->first, "b");
    ++riter;
    ensure_eq(riter->first, "a");
    ++riter;
    ensure(riter == obj.as_object().rend());
}

TEST(object_iter_postfix_increment)
{
    jsonv::value obj = jsonv::object({ { "a", 1 }, { "b", 2 } });

    auto check = [](auto iter, auto last)
    {
        auto prev = iter++;
        ensure_eq(prev->first, "a");
        ensure_eq(iter->first, "b");
        prev = iter++;
        ensure_eq(prev->first, "b");
        ensure(iter == last);
    };
    check(obj.begin_object(), obj.end_object());

    const jsonv::value& cobj = obj;
    check(cobj.begin_object(), cobj.end_object());
}

TEST(object_iter_postfix_decrement)
{
    jsonv::value obj = jsonv::object({ { "a", 1 }, { "b", 2 } });

    auto check = [](auto iter)
    {
        auto last = iter;
        auto prev = iter--;
        ensure(prev == last);
        ensure_eq(iter->first, "b");
        prev = iter--;
        ensure_eq(prev->first, "b");
        ensure_eq(iter->first, "a");
    };
    check(obj.end_object());

    const jsonv::value& cobj = obj;
    check(cobj.end_object());
}

TEST(object_compare)
{
    using namespace jsonv;

    value obj = object();
    value i = 5;

    // really just a test to see if this compiles:
    ensure(obj != i);
}

TEST(object_erase_key)
{
    jsonv::value obj = jsonv::object({ { "foo", 5 }, { "bar", "wat" } });
    ensure_eq(obj.size(), 2);
    ensure_eq(obj.count("bar"), 1);
    ensure_eq(obj.count("foo"), 1);
    ensure_eq(obj.erase("foo"), 1);
    ensure_eq(obj.count("bar"), 1);
    ensure_eq(obj.count("foo"), 0);
    ensure_eq(obj.erase("foo"), 0);
}

TEST(object_erase_iter)
{
    jsonv::value obj = jsonv::object({ { "foo", 5 }, { "bar", "wat" } });
    ensure_eq(obj.size(), 2);
    ensure_eq(obj.count("bar"), 1);
    ensure_eq(obj.count("foo"), 1);
    auto iter = obj.find("bar");
    ensure_eq(iter->first, "bar");
    iter = obj.erase(iter);
    ensure_eq(obj.count("bar"), 0);
    ensure_eq(obj.count("foo"), 1);
    ensure_eq(obj.erase("bar"), 0);
    ensure_eq(iter->first, "foo");
}

TEST(object_erase_whole)
{
    jsonv::value obj = jsonv::object({ { "foo", 5 }, { "bar", "wat" } });
    ensure_eq(obj.size(), 2);
    ensure_eq(obj.count("bar"), 1);
    ensure_eq(obj.count("foo"), 1);
    auto iter = obj.begin_object();
    ensure_eq(iter->first, "bar");
    iter = obj.erase(iter, obj.end_object());
    ensure_eq(obj.size(), 0);
    ensure_eq(obj.count("bar"), 0);
    ensure_eq(obj.count("foo"), 0);
    ensure_eq(obj.erase("bar"), 0);
    ensure(iter == obj.end_object());
    ensure(iter == obj.begin_object());
}

TEST(object_extract_move)
{
    jsonv::value obj1 = jsonv::object({ { "a", 5 }, { "b", "taco" } });
    jsonv::value obj2 = jsonv::object();

    obj2.insert(obj1.extract("b"));

    ensure_eq(obj1, jsonv::object({ { "a", 5 } }));
    ensure_eq(obj2, jsonv::object({ { "b", "taco" } }));
}

// Every other extract test goes through the by-key overloads, which look the position up and delegate here.
TEST(object_extract_iterator)
{
    jsonv::value obj = jsonv::object({ { "a", 5 }, { "b", "taco" } });

    auto handle = obj.extract(obj.find("b"));

    ensure(!handle.empty());
    ensure_eq(handle.key(), "b");
    ensure_eq(handle.mapped(), jsonv::value("taco"));

    ensure_eq(obj, jsonv::object({ { "a", 5 } }));
}

TEST(object_extract_missing_key_is_empty)
{
    jsonv::value obj = jsonv::object({ { "a", 5 } });

    ensure(obj.extract("nope").empty());
    ensure(obj.extract(L"nope").empty());

    ensure_eq(obj, jsonv::object({ { "a", 5 } }));
}

// A jsonv::value is a handle -- a kind::object value holds a pointer to a heap-allocated map. Moving the value
// transfers that pointer, so every sub-value inside keeps its address; copying allocates a fresh map whose sub-values
// live at different addresses. Address identity is therefore an exact probe for whether insert moved the pair into the
// object or deep-copied it, which is the defect behind issue #152. The addresses cannot coincide by accident in the
// copying case: the by-value `pair` parameter outlives the map insertion, so the original allocation is still live
// while the copy is made and the allocator cannot hand back the same address.
TEST(object_insert_moves_the_inserted_value)
{
    jsonv::value nested         = jsonv::object({ { "deep", "payload" } });
    const void*  address_before = &nested.at("deep");

    jsonv::value obj = jsonv::object();
    obj.insert({ "key", std::move(nested) });

    ensure_eq(address_before, static_cast<const void*>(&obj.at("key").at("deep")));
}

TEST(object_try_emplace)
{
    jsonv::value obj = jsonv::object({ { "a", 5 } });

    auto fresh = obj.try_emplace("b", "taco");
    ensure(fresh.second);
    ensure_eq(fresh.first->first, "b");
    ensure_eq(fresh.first->second, jsonv::value("taco"));

    // An existing key is left exactly as it was -- try_emplace never overwrites.
    auto dupe = obj.try_emplace("a", 9);
    ensure(!dupe.second);
    ensure_eq(dupe.first->second, jsonv::value(5));

    ensure_eq(obj, jsonv::object({ { "a", 5 }, { "b", "taco" } }));
    ensure_throws(jsonv::kind_error, jsonv::array().try_emplace("a", 1));
}

// try_emplace takes its key by const reference specifically so that a collision leaves the caller's key alone, which
// is the guarantee that distinguishes it from emplace.
TEST(object_try_emplace_does_not_consume_key_on_collision)
{
    jsonv::value obj = jsonv::object({ { "a", 5 } });
    std::string  key = "a";

    ensure(!obj.try_emplace(key, 9).second);

    ensure_eq(key, "a");
}

TEST(object_try_emplace_wide_key)
{
    jsonv::value obj = jsonv::object();

    ensure(obj.try_emplace(L"a", 1).second);
    ensure(!obj.try_emplace(L"a", 2).second);

    ensure_eq(obj, jsonv::object({ { "a", 1 } }));
}

TEST(object_insert_or_assign)
{
    jsonv::value obj = jsonv::object({ { "a", 5 } });

    auto fresh = obj.insert_or_assign("b", "taco");
    ensure(fresh.second);
    ensure_eq(fresh.first->second, jsonv::value("taco"));

    // A false `second` means "assigned", not "failed" -- the element must have been overwritten.
    auto assigned = obj.insert_or_assign("a", 9);
    ensure(!assigned.second);
    ensure_eq(assigned.first->second, jsonv::value(9));

    ensure_eq(obj, jsonv::object({ { "a", 9 }, { "b", "taco" } }));
    ensure_throws(jsonv::kind_error, jsonv::array().insert_or_assign("a", 1));
}

TEST(object_insert_or_assign_wide_key)
{
    jsonv::value obj = jsonv::object({ { "a", 1 } });

    ensure(!obj.insert_or_assign(L"a", 2).second);
    ensure(obj.insert_or_assign(L"b", 3).second);

    ensure_eq(obj, jsonv::object({ { "a", 2 }, { "b", 3 } }));
}

TEST(object_emplace)
{
    jsonv::value obj = jsonv::object();

    ensure(obj.emplace("a", 1).second);
    ensure(obj.emplace("b", 2).second);

    // Like std::map::emplace, an existing key is not overwritten.
    ensure(!obj.emplace("a", 99).second);

    ensure_eq(obj, jsonv::object({ { "a", 1 }, { "b", 2 } }));
    ensure_throws(jsonv::kind_error, jsonv::array().emplace("a", 1));
}

// A jsonv::value is a handle, so emplace must hand the mapped value to the object by move, exactly as insert does.
// See the comment on object_insert_moves_the_inserted_value for why address identity proves this.
TEST(object_emplace_moves_the_inserted_value)
{
    jsonv::value nested         = jsonv::object({ { "deep", "payload" } });
    const void*  address_before = &nested.at("deep");

    jsonv::value obj = jsonv::object();
    obj.emplace("key", std::move(nested));

    ensure_eq(address_before, static_cast<const void*>(&obj.at("key").at("deep")));
}

TEST(object_emplace_wide_key)
{
    jsonv::value obj = jsonv::object({ { "a", 5 } });

    ensure(obj.emplace(L"b", 2).second);

    // An existing key is preserved, exactly as for the narrow overload.
    ensure(!obj.emplace(L"a", 9).second);

    ensure_eq(obj, jsonv::object({ { "a", 5 }, { "b", 2 } }));
}

// The wide overloads must validate the kind of the receiver before converting the key. A key which cannot be encoded
// makes convert_to_narrow throw std::range_error, which would otherwise pre-empt the documented kind_error.
TEST(object_wide_key_on_non_object_reports_kind_error)
{
    const std::wstring lone_surrogate(1, wchar_t(0xd800));
    jsonv::value       arr = jsonv::array();

    ensure_throws(jsonv::kind_error, arr.emplace(lone_surrogate, 1));
    ensure_throws(jsonv::kind_error, arr.try_emplace(lone_surrogate, 1));
    ensure_throws(jsonv::kind_error, arr.insert_or_assign(lone_surrogate, 1));

    // On an actual object the conversion is reached, and it is the one that fails.
    ensure_throws(std::range_error, jsonv::object().try_emplace(lone_surrogate, 1));
}

#if WCHAR_MAX > 0xffff
// A wide key is UTF-16 even where wchar_t is 32 bits. Each of these used to be truncated to 16 bits per unit and so
// named some other key -- the last one a valid U+1F600 (#271).
TEST(object_wide_key_wider_than_utf16)
{
    const std::wstring keys[] =
        {
            { wchar_t(0x1f600) },
            { wchar_t(0x10000) },
            { wchar_t(0xd83d), wchar_t(0x1de00) },
        };

    for (const auto& key : keys)
    {
        jsonv::value obj = jsonv::object();

        ensure_throws(std::range_error, obj[key]);
        ensure_throws(std::range_error, obj.at(key));
        ensure_throws(std::range_error, std::as_const(obj).at(key));
        ensure_throws(std::range_error, obj.count(key));
        ensure_throws(std::range_error, obj.find(key));
        ensure_throws(std::range_error, std::as_const(obj).find(key));
        ensure_throws(std::range_error, obj.insert({ key, 1 }));
        ensure_throws(std::range_error, obj.insert(obj.end_object(), { key, 1 }));
        ensure_throws(std::range_error, obj.insert({ { key, 1 } }));
        ensure_throws(std::range_error, obj.emplace(key, 1));
        ensure_throws(std::range_error, obj.try_emplace(key, 1));
        ensure_throws(std::range_error, obj.insert_or_assign(key, 1));
        ensure_throws(std::range_error, obj.erase(key));
        ensure_throws(std::range_error, obj.extract(key));
        ensure_throws(std::range_error, jsonv::object({ { key, 1 } }));

        ensure_eq(obj, jsonv::object());
    }
}
#endif

// insert(hint, handle) documents that the handle keeps ownership of its element when the insertion does not happen.
TEST(object_insert_hint_node_handle_collision_keeps_handle)
{
    jsonv::value src = jsonv::object({ { "a", 5 } });
    jsonv::value dst = jsonv::object({ { "a", "taco" } });

    auto handle = src.extract("a");
    auto iter   = dst.insert(dst.end_object(), std::move(handle));

    ensure_eq(iter->second, jsonv::value("taco"));
    ensure(!handle.empty());
    ensure_eq(handle.key(), "a");
    ensure_eq(handle.mapped(), jsonv::value(5));
}

// The move constructor gives up the source's ownership of the element, and the move assignment operator is specified
// to match it. Issue #203 was the two disagreeing.
TEST(object_node_handle_move_construct_empties_source)
{
    jsonv::value src = jsonv::object({ { "key", "payload" } });

    auto from = src.extract("key");
    auto to   = std::move(from);

    ensure(!to.empty());
    ensure_eq(to.key(), "key");
    ensure_eq(to.mapped(), jsonv::value("payload"));

    ensure(from.empty());
    ensure(!static_cast<bool>(from));
}

TEST(object_node_handle_move_assign_empties_source)
{
    jsonv::value src = jsonv::object({ { "key", "payload" } });

    auto                      from = src.extract("key");
    jsonv::object_node_handle to;

    to = std::move(from);

    ensure(!to.empty());
    ensure_eq(to.key(), "key");
    ensure_eq(to.mapped(), jsonv::value("payload"));

    ensure(from.empty());
    ensure(!static_cast<bool>(from));
}

TEST(object_node_handle_move_assign_over_populated_destination)
{
    jsonv::value src = jsonv::object({ { "a", 5 }, { "b", "taco" } });

    auto from = src.extract("a");
    auto to   = src.extract("b");

    to = std::move(from);

    ensure(!to.empty());
    ensure_eq(to.key(), "a");
    ensure_eq(to.mapped(), jsonv::value(5));

    ensure(from.empty());
}

TEST(object_node_handle_move_assign_from_empty)
{
    jsonv::value src = jsonv::object({ { "key", "payload" } });

    auto to = src.extract("key");

    to = jsonv::object_node_handle();

    ensure(to.empty());
    ensure(!static_cast<bool>(to));
    ensure_throws(std::invalid_argument, to.key());
    ensure_throws(std::invalid_argument, to.mapped());
}

// Self-move must leave the handle owning what it already owned. The reference is what keeps the compiler from seeing
// the self-assignment and warning about it -- the same trick as move_to_self in value_tests.cpp.
TEST(object_node_handle_move_assign_to_self)
{
    jsonv::value src = jsonv::object({ { "key", "payload" } });

    auto                       handle = src.extract("key");
    jsonv::object_node_handle& same   = handle;

    handle = std::move(same);

    ensure(!handle.empty());
    ensure_eq(handle.key(), "key");
    ensure_eq(handle.mapped(), jsonv::value("payload"));
}

// A successful insert takes the element out of the handle, so the handle must stop claiming to own one.
TEST(object_insert_node_handle_empties_handle_on_success)
{
    jsonv::value src = jsonv::object({ { "a", 5 } });
    jsonv::value dst = jsonv::object();

    auto handle = src.extract("a");
    auto rc     = dst.insert(std::move(handle));

    ensure(rc.inserted);
    ensure_eq(dst, jsonv::object({ { "a", 5 } }));

    ensure(handle.empty());
    ensure(!static_cast<bool>(handle));
}

// The hint overload follows the same ownership rule as the one without a hint.
TEST(object_insert_hint_node_handle_empties_handle_on_success)
{
    jsonv::value src = jsonv::object({ { "a", 5 } });
    jsonv::value dst = jsonv::object();

    auto handle = src.extract("a");
    auto iter   = dst.insert(dst.end_object(), std::move(handle));

    ensure_eq(iter->second, jsonv::value(5));
    ensure_eq(dst, jsonv::object({ { "a", 5 } }));

    ensure(handle.empty());
}

// The mirror of object_insert_hint_node_handle_collision_keeps_handle. A key collision leaves the element where it is,
// so the handle keeps it and the returned position refers to the element which was already there.
TEST(object_insert_node_handle_collision_keeps_handle)
{
    jsonv::value src = jsonv::object({ { "a", 5 } });
    jsonv::value dst = jsonv::object({ { "a", "taco" } });

    auto handle = src.extract("a");
    auto rc     = dst.insert(std::move(handle));

    ensure(!rc.inserted);
    ensure_eq(rc.position->second, jsonv::value("taco"));
    ensure_eq(dst, jsonv::object({ { "a", "taco" } }));

    ensure(!handle.empty());
    ensure_eq(handle.key(), "a");
    ensure_eq(handle.mapped(), jsonv::value(5));
}

// An empty handle has nothing to insert. Both overloads report that with end_object() and leave the object alone.
TEST(object_insert_empty_node_handle_does_nothing)
{
    jsonv::value obj = jsonv::object({ { "a", 5 } });

    jsonv::object_node_handle handle;
    auto                      rc = obj.insert(std::move(handle));

    ensure(!rc.inserted);
    ensure(rc.position == obj.end_object());
    ensure(handle.empty());
    ensure_eq(obj, jsonv::object({ { "a", 5 } }));
}

// The hint is not end_object(), so handing it back would not pass for the documented result.
TEST(object_insert_hint_empty_node_handle_does_nothing)
{
    jsonv::value obj = jsonv::object({ { "a", 5 } });

    jsonv::object_node_handle handle;
    auto                      iter = obj.insert(obj.begin_object(), std::move(handle));

    ensure(iter == obj.end_object());
    ensure(handle.empty());
    ensure_eq(obj, jsonv::object({ { "a", 5 } }));
}

// The range insert hints at the end of the object, which is the right guess for an already-sorted source and a wrong
// one otherwise. Both paths must produce the same contents.
TEST(object_insert_range_from_sorted_map)
{
    std::map<std::string, jsonv::value> src{ { "a", 1 }, { "b", 2 }, { "c", 3 } };

    ensure_eq(jsonv::object(src.begin(), src.end()), jsonv::object({ { "a", 1 }, { "b", 2 }, { "c", 3 } }));
}

// The range insert reads end_object() to build its hint, so it now rejects a non-object even for an empty range. That
// matches insert(std::initializer_list), which has always checked the kind unconditionally.
TEST(object_insert_empty_range_on_non_object_throws)
{
    std::map<std::string, jsonv::value> src;
    jsonv::value                        arr = jsonv::array();

    ensure_throws(jsonv::kind_error, arr.insert(src.begin(), src.end()));
}

TEST(object_insert_range_interleaved_keys)
{
    jsonv::value                        obj = jsonv::object({ { "m", 0 }, { "c", 9 } });
    std::map<std::string, jsonv::value> src{ { "a", 1 }, { "c", 2 }, { "z", 3 } };

    obj.insert(src.begin(), src.end());

    // "c" was already present, so the insert of the duplicate is a no-op.
    ensure_eq(obj, jsonv::object({ { "a", 1 }, { "c", 9 }, { "m", 0 }, { "z", 3 } }));
}

TEST(object_view)
{
    const jsonv::value obj1 = jsonv::object({ { "foo", 5 }, { "bar", "wat" } });
    jsonv::value obj2 = jsonv::object();
    for (const auto& entry : obj1.as_object())
        obj2.insert(jsonv::value::object_value_type(entry));
    ensure_eq(obj1, obj2);
}

TEST(object_nested_access)
{
    jsonv::value v = jsonv::object({ { "x", 0 } });
    jsonv::value* p = &v;
    int depth = 1;
    for (std::string name : { "a", "b", "c", "d" })
    {
        (*p)[name] = jsonv::object({ { "x", depth } });
        p = &(*p)[name];
        ++depth;
    }

    ensure_eq(v["x"],                     0);
    ensure_eq(v["a"]["x"],                1);
    ensure_eq(v["a"]["b"]["x"],           2);
    ensure_eq(v["a"]["b"]["c"]["x"],      3);
    ensure_eq(v["a"]["b"]["c"]["d"]["x"], 4);
}

TEST(object_wide_nested_access)
{
    jsonv::value v = jsonv::object({ { "x", 0 } });
    jsonv::value* p = &v;
    int depth = 1;
    for (std::string name : { "a", "b", "c", "d" })
    {
        (*p)[name] = jsonv::object({ { "x", depth } });
        p = &(*p)[name];
        ++depth;
    }

    ensure_eq(v.at(L"x"),                      0);
    ensure_eq(v[L"a"][L"x"],                   1);
    ensure_eq(v[L"a"][L"b"][L"x"],             2);
    ensure_eq(v[L"a"][L"b"][L"c"][L"x"],       3);
    ensure_eq(v[L"a"][L"b"][L"c"][L"d"][L"x"], 4);
}

TEST(owning_object_view)
{
    auto view = jsonv::object({ { "a", 1 }, { "b", 2 } }).as_object();
    auto iter = view.begin();
    ensure_eq("a", iter->first);
    ++iter;
    ensure_eq("b", iter->first);
    ++iter;
    ensure(iter == view.end());
}

namespace
{

using owning_object_view = jsonv::value::owning_object_view;

// The members of a view, gathered into an object so a failure prints them.
template <typename TView>
jsonv::value members_of(TView& view)
{
    jsonv::value out = jsonv::object();
    for (const auto& member : view)
        out[member.first] = member.second;
    return out;
}

// A by-value parameter is never a candidate for NRVO, so returning it makes the view move a second time.
owning_object_view pass_through(owning_object_view view)
{
    return view;
}

}

// A view owns its container, so it cannot be copied; copying would leave the iterators on the source.
static_assert(!std::is_copy_constructible<owning_object_view>::value, "an owning view must not be copyable");
static_assert(!std::is_copy_assignable<owning_object_view>::value,    "an owning view must not be copyable");
static_assert(std::is_nothrow_move_constructible<owning_object_view>::value, "an owning view must be movable");
static_assert(std::is_nothrow_move_assignable<owning_object_view>::value,    "an owning view must be movable");

TEST(object_owning_view_move_construct)
{
    using namespace jsonv;

    const value original = object({ { "a", 1 }, { "b", 2 } });
    value copy = original;
    auto from = std::move(copy).as_object();
    owning_object_view to(std::move(from));

    ensure_eq(object({ { "a", 1 }, { "b", 2 } }), members_of(to));

    // The destination owns its own container, so a write through it reaches nothing else.
    to.begin()->second = 9;
    ensure_eq(object({ { "a", 9 }, { "b", 2 } }), members_of(to));
    ensure_eq(object({ { "a", 1 }, { "b", 2 } }), original);
}

TEST(object_owning_view_move_construct_then_destroy_source)
{
    using namespace jsonv;

    auto from = std::make_unique<owning_object_view>(object({ { "a", 1 }, { "b", 2 } }).as_object());
    owning_object_view to(std::move(*from));
    from.reset();

    ensure_eq(object({ { "a", 1 }, { "b", 2 } }), members_of(to));
    (++to.begin())->second = 8;
    ensure_eq(object({ { "a", 1 }, { "b", 8 } }), members_of(to));
}

TEST(object_owning_view_move_assign)
{
    using namespace jsonv;

    auto from = std::make_unique<owning_object_view>(object({ { "a", 1 }, { "b", 2 } }).as_object());
    auto to = object({ { "old", 0 } }).as_object();
    to = std::move(*from);
    from.reset();

    ensure_eq(object({ { "a", 1 }, { "b", 2 } }), members_of(to));
    to.begin()->second = 7;
    ensure_eq(object({ { "a", 7 }, { "b", 2 } }), members_of(to));
}

TEST(object_owning_view_move_through_function)
{
    using namespace jsonv;

    auto view = pass_through(object({ { "a", 1 }, { "b", 2 } }).as_object());
    ensure_eq(object({ { "a", 1 }, { "b", 2 } }), members_of(view));
}

TEST(object_owning_view_move_reverse_iteration)
{
    using namespace jsonv;

    auto from = std::make_unique<owning_object_view>(object({ { "a", 1 }, { "b", 2 }, { "c", 3 } }).as_object());
    owning_object_view to(std::move(*from));
    from.reset();

    auto riter = to.rbegin();
    ensure_eq(riter->first, "c");
    ++riter;
    ensure_eq(riter->first, "b");
    ++riter;
    ensure_eq(riter->first, "a");
    ++riter;
    ensure(riter == to.rend());
}

// The const members which end a reverse walk built a reverse_iterator from the const_iterator that begin() returns, so
// none of them compiled for a view whose iterator and const_iterator are different types.
TEST(object_view_const_reverse_iteration)
{
    using namespace jsonv;

    auto check = [](auto riter, auto rlast)
    {
        ensure_eq(riter->first, "c");
        ++riter;
        ensure_eq(riter->first, "b");
        ++riter;
        ensure_eq(riter->first, "a");
        ++riter;
        ensure(riter == rlast);
    };

    value obj = object({ { "a", 1 }, { "b", 2 }, { "c", 3 } });
    const auto view = obj.as_object();
    static_assert(std::is_same_v<decltype(view.rend()),  value::object_view::const_reverse_iterator>);
    static_assert(std::is_same_v<decltype(view.crend()), value::object_view::const_reverse_iterator>);
    check(view.rbegin(),  view.rend());
    check(view.crbegin(), view.crend());

    // crend() is const no matter what it is called on, so a view which is not const could not call it either.
    auto mutable_view = obj.as_object();
    check(mutable_view.crbegin(), mutable_view.crend());

    const owning_object_view owned = object({ { "a", 1 }, { "b", 2 }, { "c", 3 } }).as_object();
    check(owned.rbegin(),  owned.rend());
    check(owned.crbegin(), owned.crend());
}

// The moved-from object view must not walk the nodes it handed to the destination.
TEST(object_owning_view_moved_from_is_empty)
{
    using namespace jsonv;

    auto from = object({ { "a", 1 }, { "b", 2 } }).as_object();
    owning_object_view to(std::move(from));

    ensure(from.begin() == from.end());
    ensure(from.rbegin() == from.rend());
    ensure_eq(object(), members_of(from));

    // Moving from a view which has already been moved from must not try to look inside the empty value.
    owning_object_view again(std::move(from));
    ensure(again.begin() == again.end());
    to = std::move(again);
    ensure(to.begin() == to.end());
}

TEST(object_owning_view_self_move_assign)
{
    using namespace jsonv;

    auto view = object({ { "a", 1 }, { "b", 2 } }).as_object();
    owning_object_view& alias = view;
    view = std::move(alias);

    ensure_eq(object({ { "a", 1 }, { "b", 2 } }), members_of(view));
}

// An object constructed with wide strings should be the same as one constructed with narrow ones
TEST(object_wide_keys)
{
    auto wobj = jsonv::object({ { L"a", 1 }, { L"b", 2 } });
    auto nobj = jsonv::object({ {  "a", 1 }, {  "b", 2 } });
    ensure_eq(nobj, wobj);
}

TEST(parse_empty_object)
{
    auto obj = jsonv::parse("{}");

    ensure(obj.size() == 0);
}

TEST(parse_object_wrong_kind_keys)
{
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({1: "blah")"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({null: "blah")"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({true: "blah")"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({1.3: "blah")"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({["hi"]: "blah")"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({{}: "blah")"));
}

TEST(parse_object_stops)
{
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({"a")"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({"a" )"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({"a":)"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({"a": "blah")"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({"a": "blah",)"));
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({"a": "blah", )"));
}

TEST(parse_object_value_stops)
{
    ensure_throws(jsonv::parse_error, jsonv::parse(R"({"a": "blah)"));
}

TEST(parse_object_duplicate_keys)
{
    std::string source = R"({ "a": 1, "a": 2, "a": 3 })";

    // Default settings choose last key
    ensure_eq(jsonv::object({ { "a", 3 } }),
              jsonv::parse(source, jsonv::deserialize_options::create_default())
             );

    // Choose to ignore
    ensure_eq(jsonv::object({ { "a", 1 } }),
              jsonv::parse(source,
                           jsonv::deserialize_options::create_default()
                                .on_duplicate_key(jsonv::deserialize_options::duplicate_key_action::ignore)
                          )
             );

    // Throw
    ensure_throws(jsonv::deserialization_error,
                  jsonv::parse(source,
                               jsonv::deserialize_options::create_default()
                                    .on_duplicate_key(jsonv::deserialize_options::duplicate_key_action::exception)
                              )
                 );
}

TEST(object_iteration_structured_bindings)
{
    jsonv::value obj = jsonv::object({ { "b", 2 }, { "a", 1 } });

    std::string keys;
    for (auto&& [key, val] : obj.as_object())
    {
        keys += key;
        val = val.as_integer() * 10;
    }
    ensure_eq(std::string("ab"), keys);
    ensure_eq(jsonv::object({ { "a", 10 }, { "b", 20 } }), obj);
}

TEST(object_iterator_arrow_and_conversions)
{
    jsonv::value obj  = jsonv::object({ { "a", 1 }, { "b", 2 } });
    auto         iter = obj.find("b");
    ensure_eq(std::string("b"), iter->first);
    iter->second = "two";

    jsonv::value::const_object_iterator citer = iter;
    ensure(citer == iter);
    ensure_eq(jsonv::value("two"), citer->second);

    // Copying a member out has to be asked for.
    jsonv::value::object_value_type copy(*citer);
    ensure_eq(std::string("b"), copy.first);
    ensure_eq(jsonv::value("two"), copy.second);

    // ...which a std::map's range constructor does.
    std::map<std::string, jsonv::value> as_map(obj.begin_object(), obj.end_object());
    ensure_eq(2U, as_map.size());
    ensure_eq(jsonv::value("two"), as_map.at("b"));
}

TEST(object_iterator_random_access)
{
    jsonv::value obj = jsonv::object({ { "a", 1 }, { "b", 2 }, { "c", 3 }, { "d", 4 } });

    auto first = obj.begin_object();
    auto last  = obj.end_object();
    ensure_eq(4, last - first);
    ensure_eq(std::string("c"), (first + 2)->first);
    ensure_eq(std::string("c"), (2 + first)->first);
    ensure_eq(std::string("d"), first[3].first);
    ensure_eq(std::string("b"), (last - 3)->first);
    ensure(first < last);
    ensure(last - 1 >= first + 3);

    // The functions which dispatch on the iterator category step backward and jump.
    ensure_eq(std::string("d"), std::prev(last)->first);
    ensure_eq(std::string("b"), std::next(first)->first);
    auto iter = last;
    std::advance(iter, -3);
    ensure_eq(std::string("b"), iter->first);
    ensure_eq(4, std::distance(first, last));
}

TEST(object_iterates_in_key_order_whatever_the_insertion_order)
{
    jsonv::value obj = jsonv::object();
    for (const char* key : { "m", "c", "x", "a", "q", "b", "z" })
        obj[key] = key;

    std::string keys;
    for (const auto& member : obj.as_object())
        keys += member.first;
    ensure_eq(std::string("abcmqxz"), keys);

    // Erasing from the middle keeps the rest in order.
    auto after = obj.erase(obj.find("m"));
    ensure_eq(std::string("q"), after->first);
    ensure_eq(1U, obj.erase("b"));
    keys.clear();
    for (const auto& member : obj.as_object())
        keys += member.first;
    ensure_eq(std::string("acqxz"), keys);
}

TEST(object_insert_hint_is_only_a_hint)
{
    const jsonv::value expected = jsonv::object({ { "a", 1 }, { "b", 2 }, { "c", 3 }, { "d", 4 } });

    // Where the key goes,
    jsonv::value right = jsonv::object({ { "a", 1 }, { "c", 3 }, { "d", 4 } });
    auto         pos   = right.insert(right.find("c"), { "b", 2 });
    ensure_eq(std::string("b"), pos->first);
    ensure_eq(expected, right);

    // somewhere else,
    jsonv::value wrong = jsonv::object({ { "a", 1 }, { "c", 3 }, { "d", 4 } });
    pos = wrong.insert(wrong.end_object(), { "b", 2 });
    ensure_eq(std::string("b"), pos->first);
    ensure_eq(expected, wrong);

    // the end, for a key which belongs there,
    jsonv::value at_end = jsonv::object({ { "a", 1 }, { "b", 2 }, { "c", 3 } });
    pos = at_end.insert(at_end.end_object(), { "d", 4 });
    ensure_eq(std::string("d"), pos->first);
    ensure_eq(expected, at_end);

    // or just past a key which is already present: nothing is inserted, and the member with the key is returned.
    jsonv::value present = expected;
    pos = present.insert(present.find("c"), { "b", 20 });
    ensure_eq(std::string("b"), pos->first);
    ensure_eq(expected, present);
}

namespace
{

/// The members of a document, in document order: \a count keys in descending order, with every seventh member repeating
/// the key three before it when \a with_repeats. Each value is the member's position, so which of a repeated key's
/// values survives is plain.
std::vector<std::pair<std::string, std::int64_t>> descending_members(std::size_t count, bool with_repeats)
{
    std::vector<std::pair<std::string, std::int64_t>> out;
    for (std::size_t idx = count; idx-- > 0U; )
    {
        std::string key = std::to_string(idx);
        key.insert(0U, 6U - key.size(), '0');
        out.emplace_back("k" + key, std::int64_t(out.size()));
        if (with_repeats && out.size() % 7U == 0U)
            out.emplace_back(out[out.size() - 3U].first, std::int64_t(out.size()));
    }
    return out;
}

std::string to_text(const std::vector<std::pair<std::string, std::int64_t>>& members)
{
    std::string out = "{";
    for (const auto& [key, val] : members)
        out += (out.size() == 1U ? "\"" : ",\"") + key + "\":" + std::to_string(val);
    return out + "}";
}

/// What an object built from \a members holds under \a action, or \c std::nullopt when \a action is
/// \c exception and a key repeats; \a first_repeat is then the first key to.
std::optional<jsonv::value> expected_object(const std::vector<std::pair<std::string, std::int64_t>>& members,
                                            jsonv::deserialize_options::duplicate_key_action      action,
                                            std::string&                                         first_repeat
                                           )
{
    std::map<std::string, std::int64_t> model;
    for (const auto& [key, val] : members)
    {
        auto [pos, inserted] = model.try_emplace(key, val);
        if (inserted)
            continue;
        if (action == jsonv::deserialize_options::duplicate_key_action::exception)
        {
            first_repeat = key;
            return std::nullopt;
        }
        if (action == jsonv::deserialize_options::duplicate_key_action::replace)
            pos->second = val;
    }

    jsonv::value out = jsonv::object();
    for (const auto& [key, val] : model)
        out.insert(out.end_object(), { key, val });
    return out;
}

/// Build an object from \a members under \a action through \a build, and check it against \c expected_object.
template <typename FBuild>
void ensure_builds_object(const std::vector<std::pair<std::string, std::int64_t>>& members,
                          jsonv::deserialize_options::duplicate_key_action      action,
                          const FBuild&                                         build
                         )
{
    std::string first_repeat;
    auto        expected = expected_object(members, action, first_repeat);
    if (expected)
    {
        jsonv::value built = build();
        ensure_eq(*expected, built);

        std::string previous;
        for (const auto& member : built.as_object())
        {
            ensure(previous < member.first);
            previous = member.first;
        }
    }
    else
    {
        try
        {
            (void) build();
            ensure(false);
        }
        catch (const jsonv::deserialization_error& err)
        {
            const std::string message = R"(Duplicate key in object: ")" + first_repeat + R"(")";
            ensure(std::string(err.what()).find(message) != std::string::npos);
        }
    }
}

}

// An object whose keys arrive in descending order, with and without repeats, under every duplicate-key policy, by every
// path which builds a value from text: few enough members to be looked up one by one, and enough to need an index.
TEST(object_built_from_descending_keys)
{
    using action = jsonv::deserialize_options::duplicate_key_action;

    for (std::size_t count : { 3U, 16U, 17U, 40U, 5000U })
    {
        for (bool with_repeats : { false, true })
        {
            auto        members = descending_members(count, with_repeats);
            std::string text    = to_text(members);

            for (action on_duplicate : { action::replace, action::ignore, action::exception })
            {
                auto options = jsonv::deserialize_options::create_default().on_duplicate_key(on_duplicate);

                ensure_builds_object(members, on_duplicate, [&] { return jsonv::parse(text, options); });
                ensure_builds_object(members,
                                     on_duplicate,
                                     [&] { return jsonv::deserialize<jsonv::value>(jsonv::reader(text), options); }
                                    );
            }

            // A value_encoder keeps the last of a repeated key, as parse does by default.
            ensure_builds_object(members,
                                 action::replace,
                                 [&]
                                 {
                                     jsonv::value_encoder sink;
                                     jsonv::writer        to(sink);
                                     to.object_begin();
                                     for (const auto& [key, val] : members)
                                         to.key(key).integer(val);
                                     to.object_end();
                                     return std::move(sink).take();
                                 }
                                );
        }
    }
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

// The members of an object share one allocation, sized from the count on the tape, so a second member costs nothing.
TEST(object_parse_members_share_an_allocation)
{
    auto cost = [] (const std::string& text)
                {
                    jsonv_test::allocation_counter allocations;
                    jsonv::value                   parsed = jsonv::parse(text);
                    return allocations.count();
                };

    ensure_eq(cost(R"({"a":1})"), cost(R"({"a":1,"b":2})"));
    ensure_eq(cost(R"({"a":1})"), cost(R"({"a":1,"b":2,"c":3,"d":4,"e":5,"f":6,"g":7,"h":8})"));
}

#endif
