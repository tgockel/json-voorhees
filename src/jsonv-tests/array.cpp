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
#include <jsonv/parse.hpp>

#include <algorithm>

TEST(array)
{
    jsonv::array arr;
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
    jsonv::array val = jsonv::make_array(2, 10, "Hello, world!");
    ensure(val.get_kind() == jsonv::kind::array);
    jsonv::array& arr = val.as_array();
    ensure(arr.size() == 3);
    ensure(arr[0].as_integer() == 2);
    ensure(arr[1].as_integer() == 10);
    ensure(arr[2].as_string() == "Hello, world!");
}

TEST(parse_array)
{
    jsonv::value val = jsonv::parse("\t\n[2, 10, \"Hello, world!\"]   ");
    ensure(val.get_kind() == jsonv::kind::array);
    jsonv::array& arr = val.as_array();
    ensure(arr.size() == 3);
    ensure(arr[0].as_integer() == 2);
    ensure(arr[1].as_integer() == 10);
    ensure_eq(arr[2].as_string(), "Hello, world!");
    ensure(val == jsonv::make_array(2, 10, "Hello, world!"));
}

TEST(array_view_iter_assign)
{
    using namespace jsonv;
    
    value val = make_array(0, 1, 2, 3, 4, 5);
    array& arr = val.as_array();
    int64_t i = 0;
    for (array::const_iterator iter = arr.begin(); iter != arr.end(); ++iter)
    {
        ensure(iter->as_integer() == i);
        ++i;
    }
}

TEST(array_erase_single)
{
    using namespace jsonv;
    
    array arr = make_array(0, 1, 2, 3, 4, 5);
    ensure_eq(arr.size(), 6);
    auto iter = arr.erase(arr.begin() + 2);
    ensure_eq(iter->as_integer(), 3);
    ensure_eq(arr.size(), 5);
    ensure_eq(arr, make_array(0, 1, 3, 4, 5));
}

TEST(array_erase_multi)
{
    using namespace jsonv;
    
    array arr = make_array(0, 1, 2, 3, 4, 5);
    ensure_eq(arr.size(), 6);
    auto iter = arr.erase(arr.begin() + 2, arr.begin() + 4);
    ensure_eq(iter->as_integer(), 4);
    ensure_eq(arr.size(), 4);
    ensure_eq(arr, make_array(0, 1, 4, 5));
}

TEST(array_erase_multi_to_end)
{
    using namespace jsonv;
    
    array arr = make_array(0, 1, 2, 3, 4, 5);
    ensure_eq(arr.size(), 6);
    auto iter = arr.erase(arr.begin() + 3, arr.end());
    ensure(iter == arr.end());
    ensure_eq(arr.size(), 3);
    ensure_eq(arr, make_array(0, 1, 2));
}

TEST(array_erase_multi_from_begin)
{
    using namespace jsonv;
    
    array arr = make_array(0, 1, 2, 3, 4, 5);
    ensure_eq(arr.size(), 6);
    auto iter = arr.erase(arr.begin(), arr.end() - 3);
    ensure(iter == arr.begin());
    ensure_eq(iter->as_integer(), 3);
    ensure_eq(arr.size(), 3);
    ensure_eq(arr, make_array(3, 4, 5));
}

TEST(array_algo_sort)
{
    using namespace jsonv;
    
    array arr = make_array(9, 1, 3, 4, 2, 8, 6, 7, 0, 5);
    ensure_eq(arr.size(), 10);
    std::sort(std::begin(arr), std::end(arr));
    ensure_eq(arr, make_array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9));
}

TEST(parse_empty_array)
{
    auto v = jsonv::parse("[]");
    auto& arr = v.as_array();
    
    ensure_eq(arr.size(), 0);
}
