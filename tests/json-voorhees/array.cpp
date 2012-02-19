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

#include <json-voorhees/value.hpp>
#include <json-voorhees/parse.hpp>

TEST(array)
{
    jsonv::value val = jsonv::make_array();
    jsonv::array_view arr = val.as_array();
    arr.push_back(8.9);
    ensure(arr.size() == 1);
    ensure(arr[0].get_kind() == jsonv::kind::decimal);
    arr.push_back(true);
    ensure(arr.size() == 2);
    ensure(arr[0].get_kind() == jsonv::kind::decimal);
    ensure(arr[1].get_kind() == jsonv::kind::boolean);
    arr[0] = "Hi";
    ensure(arr.size() == 2);
    ensure(arr[0].get_kind() == jsonv::kind::string);
    ensure(arr[1].get_kind() == jsonv::kind::boolean);
}

TEST(make_array)
{
    jsonv::value val = jsonv::make_array(2, 10, "Hello, world!");
    ensure(val.get_kind() == jsonv::kind::array);
    jsonv::array_view arr = val.as_array();
    ensure(arr.size() == 3);
    ensure(arr[0].as_integer() == 2);
    ensure(arr[1].as_integer() == 10);
    ensure(arr[2].as_string() == "Hello, world!");
}

TEST(parse_array)
{
    jsonv::value val = jsonv::parse("\t\n[2, 10, \"Hello, world!\"]   ");
    ensure(val.get_kind() == jsonv::kind::array);
    jsonv::array_view arr = val.as_array();
    ensure(arr.size() == 3);
    ensure(arr[0].as_integer() == 2);
    ensure(arr[1].as_integer() == 10);
    ensure(arr[2].as_string() == "Hello, world!");
    ensure(val == jsonv::make_array(2, 10, "Hello, world!"));
}

TEST(array_view_iter_assign)
{
    using namespace jsonv;
    
    value val = make_array(0, 1, 2, 3, 4, 5);
    array_view arr = val.as_array();
    int64_t i = 0;
    for (array_view::const_iterator iter = arr.begin(); iter != arr.end(); ++iter)
    {
        ensure(iter->as_integer() == i);
        ++i;
    }
}
