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
    jsonv::object obj;
    obj["hi"] = false;
    ensure(obj["hi"].as_boolean() == false);
    obj["yay"] = jsonv::make_array("Hello", "to", "the", "world");
    ensure(obj["hi"].as_boolean() == false);
    ensure(obj["yay"].as_array().size() == 4);
}

TEST(object_view_iter_assign)
{
    using namespace jsonv;
    
    value val = make_object("foo", 5, "bar", "wat");
    std::map<std::string, bool> found{ { "foo", false }, { "bar", false } };
    object& obj = val.as_object();
    ensure(obj.size() == 2);
    
    for (object::const_iterator iter = obj.begin(); iter != obj.end(); ++iter)
        found[iter->first] = true;
    
    for (auto iter = found.begin(); iter != found.end(); ++iter)
        ensure(iter->second);
}

TEST(object_compare)
{
    using namespace jsonv;
    
    object obj;
    value i = 5;
    
    // really just a test to see if this compiles:
    ensure(obj != i);
}

TEST(object_erase_key)
{
    jsonv::object obj = jsonv::make_object("foo", 5, "bar", "wat");
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
    jsonv::object obj = jsonv::make_object("foo", 5, "bar", "wat");
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
    jsonv::object obj = jsonv::make_object("foo", 5, "bar", "wat");
    ensure_eq(obj.size(), 2);
    ensure_eq(obj.count("bar"), 1);
    ensure_eq(obj.count("foo"), 1);
    auto iter = obj.begin();
    ensure_eq(iter->first, "bar");
    iter = obj.erase(iter, obj.end());
    ensure_eq(obj.size(), 0);
    ensure_eq(obj.count("bar"), 0);
    ensure_eq(obj.count("foo"), 0);
    ensure_eq(obj.erase("bar"), 0);
    ensure(iter == obj.end());
    ensure(iter == obj.begin());
}

TEST(parse_empty_object)
{
    auto v = jsonv::parse("{}");
    auto& obj = v.as_object();
    
    ensure(obj.size() == 0);
}
