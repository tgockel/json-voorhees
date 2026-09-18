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
#include <jsonv/serialization.hpp>

#include <map>
#include <stdexcept>
#include <string>
#include <utility>

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
        obj2.insert(entry);
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
              jsonv::parse(source, jsonv::extract_options::create_default())
             );

    // Choose to ignore
    ensure_eq(jsonv::object({ { "a", 1 } }),
              jsonv::parse(source,
                           jsonv::extract_options::create_default()
                                .on_duplicate_key(jsonv::extract_options::duplicate_key_action::ignore)
                          )
             );

    // Throw
    ensure_throws(jsonv::extraction_error,
                  jsonv::parse(source,
                               jsonv::extract_options::create_default()
                                    .on_duplicate_key(jsonv::extract_options::duplicate_key_action::exception)
                              )
                 );
}
