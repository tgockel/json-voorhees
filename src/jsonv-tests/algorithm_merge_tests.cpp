/** \file
 *  Data-driven tests for testing merges.
 *
 *  Copyright (c) 2015-2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"
#include "filesystem_util.hpp"

#include <jsonv/algorithm.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/value.hpp>

#include <fstream>
#include <stdexcept>

namespace jsonv_test
{

TEST(merge_single)
{
    jsonv::value x = jsonv::merge(jsonv::null);
    ensure_eq(jsonv::null, x);
}

namespace
{

/// What a \c jsonv::dynamic_merge_rules built by \c counting_rules was called with: how often each function ran, and
/// the arguments of the latest call to either.
struct merge_calls
{
    int          same_key      = 0;
    int          type_conflict = 0;
    jsonv::path  path;
    jsonv::value a;
    jsonv::value b;
};

jsonv::dynamic_merge_rules counting_rules(merge_calls& calls)
{
    return jsonv::dynamic_merge_rules(
        [&calls] (jsonv::path&& p, jsonv::value&& a, jsonv::value&& b)
        {
            ++calls.same_key;
            calls.path = std::move(p);
            calls.a    = std::move(a);
            calls.b    = std::move(b);
            return jsonv::value("same-key");
        },
        [&calls] (jsonv::path&& p, jsonv::value&& a, jsonv::value&& b)
        {
            ++calls.type_conflict;
            calls.path = std::move(p);
            calls.a    = std::move(a);
            calls.b    = std::move(b);
            return jsonv::value("type-conflict");
        }
    );
}

}

TEST(merge_dynamic_type_conflict)
{
    // dynamic_merge_rules used to call its same_key function here and never touch type_conflict
    merge_calls calls;
    jsonv::dynamic_merge_rules rules = counting_rules(calls);

    jsonv::value result = jsonv::merge_explicit(rules, jsonv::path::create(".x"), jsonv::value(1), jsonv::value("x"));
    ensure_eq(jsonv::value("type-conflict"), result);
    ensure_eq(0, calls.same_key);
    ensure_eq(1, calls.type_conflict);
    ensure_eq(jsonv::path::create(".x"), calls.path);
    ensure_eq(jsonv::value(1), calls.a);
    ensure_eq(jsonv::value("x"), calls.b);
}

TEST(merge_dynamic_same_key)
{
    merge_calls calls;
    jsonv::dynamic_merge_rules rules = counting_rules(calls);

    jsonv::value result = jsonv::merge_explicit(rules,
                                                jsonv::path(),
                                                jsonv::object({ { "a", 1 } }),
                                                jsonv::object({ { "a", 2 } })
                                               );
    ensure_eq(jsonv::object({ { "a", "same-key" } }), result);
    ensure_eq(1, calls.same_key);
    ensure_eq(0, calls.type_conflict);
    ensure_eq(jsonv::path::create(".a"), calls.path);
    ensure_eq(jsonv::value(1), calls.a);
    ensure_eq(jsonv::value(2), calls.b);
}

TEST(merge_dynamic_integer_decimal)
{
    // An integer and a decimal are compatible, so neither function has anything to resolve
    merge_calls calls;
    jsonv::dynamic_merge_rules rules = counting_rules(calls);

    ensure_eq(jsonv::value(3.5), jsonv::merge_explicit(rules, jsonv::path(), jsonv::value(1), jsonv::value(2.5)));
    ensure_eq(jsonv::value(3.5), jsonv::merge_explicit(rules, jsonv::path(), jsonv::value(2.5), jsonv::value(1)));
    ensure_eq(0, calls.same_key);
    ensure_eq(0, calls.type_conflict);
}

TEST(merge_dynamic_type_conflict_throws)
{
    // The point of a type_conflict function which throws is that nothing else gets to produce a result
    int same_key_calls = 0;
    jsonv::dynamic_merge_rules rules(
        [&same_key_calls] (jsonv::path&&, jsonv::value&&, jsonv::value&&) -> jsonv::value
        {
            ++same_key_calls;
            return jsonv::value("same-key");
        },
        [] (jsonv::path&&, jsonv::value&&, jsonv::value&&) -> jsonv::value
        {
            throw std::runtime_error("type conflict");
        }
    );

    ensure_throws(std::runtime_error, jsonv::merge_explicit(rules, jsonv::path(), jsonv::value(1), jsonv::value("x")));
    ensure_eq(0, same_key_calls);
}

template <typename TMergeRules>
class json_merge_test :
        public unit_test
{
public:
    json_merge_test(const std::string& test_name,
                    jsonv::value       a,
                    jsonv::value       b,
                    jsonv::value       expected
                   ) :
            unit_test(std::string("merge_test/") + test_name),
            a(std::move(a)),
            b(std::move(b)),
            expected(std::move(expected))
    { }

    virtual void run_impl() override
    {
        bool expect_failure = expected.kind() == jsonv::kind::string && expected.as_string() == "FAILURE";

        try
        {
            TMergeRules rules;
            jsonv::value result = jsonv::merge_explicit(rules, jsonv::path(), a, b);
            ensure(!expect_failure);
            ensure_eq(expected, result);
        }
        catch (...)
        {
            if (!expect_failure)
                throw;
        }
    }

private:
    jsonv::value a;
    jsonv::value b;
    jsonv::value expected;
};

class json_merge_test_initializer
{
public:
    explicit json_merge_test_initializer(const std::string& rootpath)
    {
        recursive_directory_for_each(rootpath, ".json", [this] (const std::string& p)
        {
            jsonv::value whole = [&p] { std::ifstream in(p.c_str()); return jsonv::parse(in); }();
            jsonv::value a = whole.at("a");
            jsonv::value b = whole.at("b");

            checked_add<jsonv::throwing_merge_rules>(p, "expected", whole, a, b);
            checked_add<jsonv::recursive_merge_rules>(p, "recursive", whole, a, b);
        });
    }

private:
    template <typename TMergeRules>
    void checked_add(const std::string&  p,
                     const std::string&  test_name,
                     const jsonv::value& whole,
                     const jsonv::value& a,
                     const jsonv::value& b
                    )
    {
        if (whole.count(test_name))
        {
            jsonv::value expected = whole.at(test_name);
            _tests.emplace_back(new json_merge_test<TMergeRules>
                                    (
                                        filename(p) + "/" + test_name,
                                        a,
                                        b,
                                        expected
                                    )
                                );
        }
    }

private:
    std::deque<std::unique_ptr<unit_test>> _tests;
} json_merge_test_initializer_instance(test_path("merges"));

}
