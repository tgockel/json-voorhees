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
#include "filesystem_util.hpp"
#include "locale_util.hpp"

#include <jsonv/algorithm.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/path.hpp>
#include <jsonv/value.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>

namespace jsonv_test
{

using namespace jsonv;

TEST(path_kind_encoding)
{
    ensure_eq(to_string(path_element_kind::array_index), "array_index");
    ensure_eq(to_string(path_element_kind::object_key),  "object_key");
    
    (void) to_string(static_cast<path_element_kind>(~0));
}

TEST(path_element_copy_compares)
{
    path_element elem1("hi");
    path_element elem2(1);
    path_element elem3(1);
    path_element elem4(elem1);
    path_element elem5(elem2);
    
    ensure_eq(elem1, elem1);
    ensure_ne(elem1, elem2);
    ensure_ne(elem1, elem3);
    ensure_eq(elem1, elem4);
    ensure_ne(elem1, elem5);
    
    ensure_ne(elem2, elem1);
    ensure_eq(elem2, elem2);
    ensure_eq(elem2, elem3);
    ensure_ne(elem2, elem4);
    ensure_eq(elem2, elem5);
    
    ensure_ne(elem3, elem1);
    ensure_eq(elem3, elem2);
    ensure_eq(elem3, elem3);
    ensure_ne(elem3, elem4);
    ensure_eq(elem3, elem5);
    
    ensure_eq(elem4, elem1);
    ensure_ne(elem4, elem2);
    ensure_ne(elem4, elem3);
    ensure_eq(elem4, elem4);
    ensure_ne(elem4, elem5);
    
    ensure_ne(elem5, elem1);
    ensure_eq(elem5, elem2);
    ensure_eq(elem5, elem3);
    ensure_ne(elem5, elem4);
    ensure_eq(elem5, elem5);
    
    elem3 = elem1;
    elem1 = elem2;
    
    ensure_eq(elem1, elem1);
    ensure_eq(elem1, elem2);
    ensure_ne(elem1, elem3);
    
    ensure_eq(elem2, elem1);
    ensure_eq(elem2, elem2);
    ensure_ne(elem2, elem3);
    
    ensure_ne(elem3, elem1);
    ensure_ne(elem3, elem2);
    ensure_eq(elem3, elem3);
}

TEST(path_assign)
{
    path p;
    path q = path::create(".b[5][2].crap[\"blah\"]");
    const path golden = path({ "b", 5, 2, "crap", "blah" });
    p = q;
    ensure_eq(p, q);
    ensure_eq(golden, p);
    ensure_eq(golden, q);
    
    q = std::move(p);
    ensure_eq(0, p.size());
    ensure_eq(golden.size(), q.size());
    
    p = std::move(q);
    ensure_eq(golden.size(), p.size());
    ensure_eq(0, q.size());
    
    q = p;
    ensure_eq(p, q);
    ensure_eq(golden, p);
    ensure_eq(golden, q);
    p = q;
    ensure_eq(p, q);
    ensure_eq(golden, p);
    ensure_eq(golden, q);
}

TEST(path_concat_key)
{
    path p({ path_element("a") });
    path q = p + "b";
    ensure_eq(q, path({ path_element("a"), path_element("b") }));
    ensure_eq(to_string(q), ".a.b");
}

TEST(path_traverse)
{
    value tree;
    {
        std::ifstream stream(test_path("paths.json").c_str());
        tree = parse(stream);
    }
    
    traverse(tree,
             [this, &tree] (const path& p, const value& x)
             {
                 ensure_eq(to_string(p), x.as_string());
                 auto q = path::create(x.as_string());
                 ensure_eq(p, q);
                 ensure_eq(x, tree.at_path(p));
                 ensure_eq(x, tree.at_path(q));
             },
             true
            );
}

TEST(path_append_key)
{
    path p;
    p += "a";
    path q({ path_element("a") });
    ensure_eq(p, q);
}

TEST(path_create_simplestring)
{
    path p = path::create(".a.b.c");
    path q({ "a", "b", "c" });
    ensure_eq(p, q);
}

TEST(path_value_construction)
{
    value tree;
    tree.path(".a.b[2]") = "Hello!";
    tree.path(".a[\"b\"][3]") = 3;
    tree.path(".a[\"b\"][1]") = "Yo";
    
    value expected = object({
                             { "a", object({
                                            { "b", array({ null, "Yo", "Hello!", 3 }) }
                                          })
                             }
                           });
    ensure_eq(expected, tree);
}

TEST(value_at_path_out_of_range)
{
    value tree;
    tree.path(".a.b[2]") = "Hello!";
    tree.path(".a[\"b\"][3]") = 3;
    tree.path(".a[\"b\"][1]") = "Yo";
    ensure_eq(1UL, tree.count_path(".a.b[0]"));
    ensure_throws(std::out_of_range, tree.at_path(".does.not.exist"));
    ensure_eq(0UL, tree.count_path(".does.not.exist"));
    const value& tree2 = tree;
    ensure_throws(std::out_of_range, tree2.at_path(".does.not.exist"));
}

TEST(value_path_parse_invalid)
{
    value tree = object({ { "a", 1 } });
    const value& ctree = tree;
    ensure_throws(std::invalid_argument, tree.at_path(".a#"));
    ensure_throws(std::invalid_argument, ctree.at_path(".a#"));
    ensure_throws(std::invalid_argument, ctree.count_path(".a#"));
    ensure_throws(std::invalid_argument, tree.path(".a#"));
}

TEST(value_path_array_construct)
{
    value arr;
    arr.path(0) = 0;
    arr.path(1) = 1;
    arr.path(2) = 2;
    ensure_eq(arr, array({ 0, 1, 2 }));
    ensure_eq(1UL, arr.count_path(0));
    ensure_eq(1UL, arr.count_path(1));
    ensure_eq(1UL, arr.count_path(2));
    ensure_eq(0UL, arr.count_path(3));
    ensure_eq(0UL, arr.count_path(".a"));
}

TEST(path_element_access)
{
    path p = path::create(".a.b[5]");
    ensure_throws(std::out_of_range, p.at(4));
    ensure_throws(kind_error, p.at(0).index());
    ensure_throws(kind_error, p.at(2).key());
    
    path q = std::move(p);
    p = q;
    ensure_eq(p, q);
}

TEST(path_element_kind_to_string_invalid)
{
    (void) to_string(static_cast<path_element_kind>(~0));
}

TEST(path_parse_invalid)
{
    ensure_throws(std::invalid_argument, path::create(".a#"));
    ensure_throws(std::invalid_argument, path::create("2"));
}

TEST(path_parse_unpaired_surrogate)
{
    // Matching a key only checks that each `\u` has four hex digits, so these get as far as the decoder (#285)
    ensure_throws(std::invalid_argument, path::create(R"(["\ud800"])"));       // a lone high surrogate
    ensure_throws(std::invalid_argument, path::create(R"(["\udc00"])"));       // a lone low surrogate
    ensure_throws(std::invalid_argument, path::create(R"(["\ud800\u0041"])")); // a high surrogate, then an `A`

    try
    {
        (void) path::create(R"(.a["\udc00"])");
        ensure(!"std::invalid_argument was not thrown");
    }
    catch (const std::invalid_argument& ex)
    {
        ensure_eq(std::string(R"(Invalid specification ".a["\udc00"]". Syntax error at "["\udc00"]": )"
                              R"(unpaired low surrogate (\udc00))"
                             ),
                  std::string(ex.what())
                 );
    }
}

TEST(path_combines)
{
    path goal = path::create(".a.b.c[3][4][5]");
    path a = path::create(".a.b.c");
    path b = path::create("[3][4][5]");
    
    ensure_eq(goal, a + b);
    a += b;
    ensure_eq(goal, a);
}

TEST(path_to_string_round_trips)
{
    const std::string keys[] =
    {
        "abc", "_", "$", "_a$1",
        // Not identifiers, so these need brackets
        "123", "1a", "", "a b", "a.b", "a[0]", "]", "[\"x\"]",
        // Characters a JSON string has to escape, and a backslash which only looks like an escape
        "a\"b", "a\\b", "a/b", "\\u0041", "\t\n\b\f\r", std::string("a\0b", 3), "\x01", "\x7f",
        // Well-formed UTF-8 of each length
        "\xc3\xa9", "\xe6\x97\xa5\xe6\x9c\xac", "\xf0\x9f\x98\x80",
    };

    for (const std::string& key : keys)
    {
        const path p({ key });
        ensure_eq(p, path::create(to_string(p)));

        const path q({ "a", key, 1000, key });
        ensure_eq(q, path::create(to_string(q)));
    }

    const path indices({ std::size_t(0), std::numeric_limits<std::size_t>::max() });
    ensure_eq(indices, path::create(to_string(indices)));
}

TEST(path_to_string_forms)
{
    ensure_eq(".abc",                  to_string(path({ "abc" })));
    ensure_eq("._a$1",                 to_string(path({ "_a$1" })));
    ensure_eq(".$",                    to_string(path({ "$" })));
    ensure_eq(R"(["123"])",            to_string(path({ "123" })));
    ensure_eq(R"([""])",               to_string(path({ "" })));
    ensure_eq(R"(["a b"])",            to_string(path({ "a b" })));
    ensure_eq(R"(["a\"b"])",           to_string(path({ "a\"b" })));
    ensure_eq(R"(["a\\b"])",           to_string(path({ "a\\b" })));
    ensure_eq(R"(["a/b"])",            to_string(path({ "a/b" })));
    ensure_eq(R"(["\u0001"])",         to_string(path({ "\x01" })));
    ensure_eq("[\"\xc3\xa9\"]",        to_string(path({ "\xc3\xa9" })));
    ensure_eq(R"(.a[1000]["b c"][0])", to_string(path({ "a", 1000, "b c", 0 })));
}

TEST(path_to_string_ignores_locale)
{
    global_grouping_locale grouping;

    const path p({ "a", 1000, 1234567 });
    ensure_eq(".a[1000][1234567]", to_string(p));
    ensure_eq(p, path::create(to_string(p)));
}

TEST(path_element_to_string)
{
    const path_element index(std::size_t(3));
    const path_element identifier("abc");
    path_element       bracketed("a b");
    const path         p({ "abc", 3 });

    ensure_eq("[3]",        jsonv::to_string(index));
    ensure_eq(".abc",       jsonv::to_string(identifier));
    ensure_eq(R"(["a b"])", jsonv::to_string(bracketed));
    ensure_eq(".abc",       jsonv::to_string(p[0]));
    ensure_eq("[3]",        jsonv::to_string(p.at(1)));
    ensure_eq(".abc[3]",    jsonv::to_string(p));

    {
        using jsonv::to_string;
        ensure_eq("[3]",        to_string(index));
        ensure_eq(R"(["a b"])", to_string(bracketed));
        ensure_eq(".abc[3]",    to_string(p));
    }
}

TEST(path_element_to_string_leaves_conversions_to_value)
{
    // `path_element` converts from numbers and strings as `value` does. While `to_string` of a `path_element` was a
    // plain function, each of these was ambiguous wherever `path.hpp` was included. They mean the `value`, whose text
    // is not the element's: `5` rather than `[5]`, `"x"` rather than `.x`.
    ensure_eq(jsonv::to_string(value(5)),                     jsonv::to_string(5));
    ensure_eq(jsonv::to_string(value(std::int64_t(5))),       jsonv::to_string(std::int64_t(5)));
    ensure_eq(jsonv::to_string(value(std::size_t(5))),        jsonv::to_string(std::size_t(5)));
    ensure_eq(jsonv::to_string(value(1.5)),                   jsonv::to_string(1.5));
    ensure_eq(jsonv::to_string(value(true)),                  jsonv::to_string(true));
    ensure_eq(jsonv::to_string(value(std::string("x"))),      jsonv::to_string(std::string("x")));
    ensure_eq(jsonv::to_string(value(std::string_view("x"))), jsonv::to_string(std::string_view("x")));
    ensure_eq(jsonv::to_string(value("x")),                   jsonv::to_string("x"));
    ensure_eq("5",                                            jsonv::to_string(5));
    ensure_eq(R"("x")",                                       jsonv::to_string("x"));
}

TEST(path_parse_index_overflow)
{
    const std::string max = std::to_string(std::numeric_limits<std::size_t>::max());
    ensure_eq(path({ std::numeric_limits<std::size_t>::max() }), path::create("[" + max + "]"));

    // The largest `size_t` ends in a 5 whether it has 32 bits or 64, so this is one more than it
    std::string over = max;
    ++over.back();
    ensure_throws(std::invalid_argument, path::create("[" + over + "]"));
    ensure_throws(std::invalid_argument, path::create("[" + max + "0]"));
    ensure_throws(std::invalid_argument, path::create("[99999999999999999999999]"));
}

}
