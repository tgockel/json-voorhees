/** \file
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"

#include <jsonv/array.hpp>
#include <jsonv/object.hpp>
#include <jsonv/parse.hpp>

#include <map>

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
    std::map<std::string, bool> found{ { "foo", false }, { "bar", false } };
    ensure(obj.size() == 2);
    
    for (auto iter = obj.begin_object(); iter != obj.end_object(); ++iter)
        found[iter->first] = true;
    
    for (auto iter = found.begin(); iter != found.end(); ++iter)
        ensure(iter->second);
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

TEST(object_view)
{
    const jsonv::value obj1 = jsonv::object({ { "foo", 5 }, { "bar", "wat" } });
    jsonv::value obj2 = jsonv::object();
    for (const std::pair<const std::string, const jsonv::value>& entry : obj1.as_object())
        obj2.insert(entry);
    ensure_eq(obj1, obj2);
}

TEST(parse_empty_object)
{
    auto obj = jsonv::parse("{}");
    
    ensure(obj.size() == 0);
}
