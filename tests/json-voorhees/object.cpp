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

#include <json-voorhees/array.hpp>
#include <json-voorhees/object.hpp>
#include <json-voorhees/parse.hpp>

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

TEST(parse_empty_object)
{
    auto v = jsonv::parse("{}");
    auto& obj = v.as_object();
    
    ensure(obj.size() == 0);
}
