/** \file
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"

#include <json-voorhees/array.hpp>
#include <json-voorhees/parse.hpp>
#include <json-voorhees/object.hpp>

#include <iostream>

using namespace jsonv;

static const object simple_obj = make_object("foo", 4,
                                             "bar", make_array(2, 3, 4, "5"),
                                             "raz", make_object()
                                            );

TEST(parse_object_simple_no_spaces)
{
    value result = parse("{\"foo\":4,\"bar\":[2,3,4,\"5\"],\"raz\":{}}");
    ensure_eq(simple_obj, result);
}

TEST(parse_object_simple_no_newlines)
{
    value result = parse("{\"foo\": 4, \"raz\": {   }, \"bar\": [ 2, 3, 4, \"5\"]}");
    ensure_eq(simple_obj, result);
}

TEST(parse_object_simple_spaces_and_tabs)
{
    value result = parse("           {    \t       \"foo\" :                 \t            4  , "
                         " \"bar\"                                :               [          \t         2\t,"
                         " 3\t\t\t,\t4                ,                    \"5\"             ]"
                         "         \t\"raz\"      \t: {                                                   }"
                         " \t\t}            \t");
    ensure_eq(simple_obj, result);
}

TEST(parse_object_simple_newlines)
{
    value result = parse(R"({
    "foo":4,
    "bar":[2,3,4,"5"],
    "raz":{}
})");
    ensure_eq(simple_obj, result);
}

TEST(parse_object_simple_general_havoc)
{
    value result = parse(R"(
        {
                                "foo"
:
                4,"raz"
                                                                        :
{
    
    
    
    
        },"bar"       :
              [
               2,
                         3,4,
                      "5"
]}
        
        
    )");
    ensure_eq(simple_obj, result);
}

TEST(parse_object_nested_single)
{
    value result = parse(R"({"a": {"b": 10}, "c":25})");
    value expected = make_object("a", make_object("b", 10),
                                 "c", 25
                                );
    ensure_eq(expected, result);
}

TEST(parse_object_empties_in_array)
{
    value result = parse(R"({"a": {"b": 10}, "c": 23.9, "d": [{"e": {}, "f": 41.4, "g": null, "h": 5}, {"i":null}]})");
    value expected = make_object("a", make_object("b", 10),
                                 "c", 23.9,
                                 "d", make_array(make_object("e", make_object(),
                                                             "f", 41.4,
                                                             "g", nullptr,
                                                             "h", 5
                                                            ),
                                                 make_object("i", nullptr)
                                                )
                                );
    ensure_eq(expected, result);
}

TEST(parse_null)
{
    value result = parse("null");
    value expected = value(nullptr);
    ensure_eq(expected, result);
}

TEST(parse_null_in_arr)
{
    value result = parse("[null,4]");
    value expected = make_array(nullptr, 4);
    ensure_eq(expected, result);
}

TEST(parse_null_in_obj)
{
    value result = parse(R"({"a": null})");
    value expected = make_object("a", nullptr);
    ensure_eq(expected, result);
}

TEST(parse_object_in_array)
{
    value result = parse(R"({"a": null, "b": {"c": 1, "d": "e", "f": null, "g": 2, )"
                         R"("h": [{"i": 3, "j": null, "k": "l", "m": 4, "n": "o", "p": "q", "r": 5}, )"
                         R"({"s": 6, "t": 7, "u": null}, {"v": "w"}]}})"
                        );
    value expected = make_object("a", nullptr,
                                 "b", make_object("c", 1,
                                                  "d", "e",
                                                  "f", nullptr,
                                                  "g", 2,
                                                  "h", make_array(make_object("i", 3,
                                                                              "j", nullptr,
                                                                              "k", "l",
                                                                              "m", 4,
                                                                              "n", "o",
                                                                              "p", "q",
                                                                              "r", 5
                                                                             ),
                                                                  make_object("s", 6,
                                                                              "t", 7,
                                                                              "u", nullptr
                                                                             ),
                                                                  make_object("v", "w")
                                                                 )
                                                 )
                                );
    ensure_eq(expected, result);
}
