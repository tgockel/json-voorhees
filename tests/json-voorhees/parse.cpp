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

using namespace jsonv;

static const object simple_obj = make_object("foo", 4,
                                             "bar", make_array(2, 3, 4, "5")
                                            );

TEST(parse_object_simple_no_spaces)
{
    value result = parse("{\"foo\":4,\"bar\":[2,3,4,\"5\"]}");
    ensure_eq(simple_obj, result);
}

TEST(parse_object_simple_no_newlines)
{
    value result = parse("{\"foo\": 4, \"bar\": [ 2, 3, 4, \"5\"]}");
    ensure_eq(simple_obj, result);
}

TEST(parse_object_simple_spaces_and_tabs)
{
    value result = parse("           {    \t       \"foo\" :                 \t            4  , "
                         " \"bar\"                                :               [          \t         2\t,"
                         " 3\t\t\t,\t4                ,                    \"5\"             ]"
                         " \t\t}            \t");
    ensure_eq(simple_obj, result);
}

TEST(parse_object_simple_newlines)
{
    value result = parse(R"({
    "foo":4,
    "bar":[2,3,4,"5"]
})");
    ensure_eq(simple_obj, result);
}

TEST(parse_object_simple_general_havoc)
{
    value result = parse(R"(
        {
                                "foo"
:
                4,
    "bar"       :
              [
               2,
                         3,4,
                      "5"
]}
        
        
    )");
    ensure_eq(simple_obj, result);
}