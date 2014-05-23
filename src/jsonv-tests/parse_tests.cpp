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
#include "pull_parser_tests.hpp"

#include <jsonv/array.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/object.hpp>

#include <iostream>

using namespace jsonv;

static const value simple_obj = object({ { "foo", 4 },
                                         { "bar", array({ 2, 3, 4, "5" }) },
                                         { "raz", object() }
                                       });

#define TEST_PARSE(basename_)                                                                                   \
    class basename_ ## _test :                                                                                  \
            public ::jsonv_test::unit_test                                                                      \
    {                                                                                                           \
    public:                                                                                                     \
        using parse_func = jsonv::value (*)(const std::string&, const jsonv::parse_options&);                   \
                                                                                                                \
        parse_func parse_impl;                                                                                  \
                                                                                                                \
        basename_ ## _test(const char* name, parse_func func) :                                                 \
                ::jsonv_test::unit_test(name),                                                                  \
                parse_impl(func)                                                                                \
        { }                                                                                                     \
                                                                                                                \
        jsonv::value parse(const std::string& input, const jsonv::parse_options& opts = jsonv::parse_options()) \
        {                                                                                                       \
            return parse_impl(input, opts);                                                                     \
        }                                                                                                       \
                                                                                                                \
        void run_impl();                                                                                        \
    } basename_ ## _direct_test_instance("parse_" #basename_, jsonv::parse),                                    \
      basename_ ## _stream_test_instance("parse_stream_" #basename_, jsonv_test::pull_parse);                   \
                                                                                                                \
    void basename_ ## _test::run_impl()

TEST_PARSE(object_simple_no_spaces)
{
    value result = parse("{\"foo\":4,\"bar\":[2,3,4,\"5\"],\"raz\":{}}");
    ensure_eq(simple_obj, result);
}

TEST_PARSE(object_simple_no_newlines)
{
    value result = parse("{\"foo\": 4, \"raz\": {   }, \"bar\": [ 2, 3, 4, \"5\"]}");
    ensure_eq(simple_obj, result);
}

TEST_PARSE(object_simple_spaces_and_tabs)
{
    value result = parse("           {    \t       \"foo\" :                 \t            4  , "
                         " \"bar\"                                :               [          \t         2\t,"
                         " 3\t\t\t,\t4                ,                    \"5\"             ]"
                         "         \t\"raz\"      \t: {                                                   }"
                         " \t\t}            \t");
    ensure_eq(simple_obj, result);
}

TEST_PARSE(object_simple_newlines)
{
    value result = parse(R"({
    "foo":4,
    "bar":[2,3,4,"5"],
    "raz":{}
})");
    ensure_eq(simple_obj, result);
}

TEST_PARSE(object_simple_general_havoc)
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

TEST_PARSE(object_nested_single)
{
    value result = parse(R"({"a": {"b": 10}, "c":25})");
    value expected = object({ { "a", object({ { "b", 10 } }) },
                              { "c", 25 }
                           });
    ensure_eq(expected, result);
}

TEST_PARSE(object_empties_in_array)
{
    value result = parse(R"({"a": {"b": 10}, "c": 23.9, "d": [{"e": {}, "f": 41.4, "g": null, "h": 5}, {"i":null}]})");
    value expected = object({ { "a", object({ { "b", 10 } }) },
                              { "c", 23.9 },
                              { "d", array({ object({ { "e", object() },
                                                      { "f", 41.4 },
                                                      { "g", nullptr },
                                                      { "h", 5 },
                                                   }),
                                             object({ { "i", nullptr } })
                                          })
                              }
                           });
    ensure_eq(expected, result);
}

TEST_PARSE(null)
{
    value result = parse("null");
    value expected = value(nullptr);
    ensure_eq(expected, result);
}

TEST_PARSE(null_in_arr)
{
    value result = parse("[null,4]");
    value expected = array({ nullptr, 4 });
    ensure_eq(expected, result);
}

TEST_PARSE(null_in_obj)
{
    value result = parse(R"({"a": null})");
    value expected = object({ { "a", nullptr } });
    ensure_eq(expected, result);
}

TEST_PARSE(object_in_array)
{
    value result = parse(R"({"a": null, "b": {"c": 1, "d": "e", "f": null, "g": 2, )"
                         R"("h": [{"i": 3, "j": null, "k": "l", "m": 4, "n": "o", "p": "q", "r": 5}, )"
                         R"({"s": 6, "t": 7, "u": null}, {"v": "w"}]}})"
                        );
    value expected = object({ { "a", nullptr },
                              { "b", object({ { "c", 1 },
                                              { "d", "e" },
                                              { "f", nullptr },
                                              { "g", 2 },
                                              { "h", array({ object({ { "i", 3 },
                                                                      { "j", nullptr },
                                                                      { "k", "l" },
                                                                      { "m", 4 },
                                                                      { "n", "o" },
                                                                      { "p", "q" },
                                                                      { "r", 5 },
                                                                   }),
                                                             object({ { "s", 6 },
                                                                      { "t", 7 },
                                                                      { "u", nullptr },
                                                                   }),
                                                             object({ { "v", "w" } })
                                                          })
                                              },
                                           })
                              }
                           });
    ensure_eq(expected, result);
}

TEST_PARSE(malformed_decimal)
{
    ensure_throws(jsonv::parse_error, parse("123.456.789"));
}

TEST_PARSE(malformed_decimal_collect_all)
{
    auto options = jsonv::parse_options()
                       .failure_mode(jsonv::parse_options::on_error::collect_all);
    ensure_throws(jsonv::parse_error, parse("123.456.789", options));
}

TEST_PARSE(malformed_decimal_ignore)
{
    auto options = jsonv::parse_options()
                       .failure_mode(jsonv::parse_options::on_error::ignore);
    // Could potentially check that the result is still a decimal, but the result is undefined.
    parse("123.456.789", options);
}
