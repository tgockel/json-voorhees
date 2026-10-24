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

#include <cstdint>
#include <fstream>
#include <limits>
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

namespace
{

constexpr std::int64_t integer_max = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t integer_min = std::numeric_limits<std::int64_t>::min();

}

// An integer and a decimal which hold the same number compare equal, so the kind is what says whether a sum wrapped
// or was promoted: INT64_MIN + -1 rounds to exactly the decimal -2^63, which is equal to INT64_MIN as an integer.
TEST(merge_integer_overflow_to_decimal)
{
    jsonv::value over = jsonv::merge(jsonv::value(integer_max), jsonv::value(1));
    ensure_eq(jsonv::kind::decimal, over.kind());
    ensure_eq(9223372036854775808.0, over.as_decimal());

    jsonv::value over_left = jsonv::merge(jsonv::value(1), jsonv::value(integer_max));
    ensure_eq(jsonv::kind::decimal, over_left.kind());
    ensure_eq(9223372036854775808.0, over_left.as_decimal());

    jsonv::value over_both = jsonv::merge(jsonv::value(integer_max), jsonv::value(integer_max));
    ensure_eq(jsonv::kind::decimal, over_both.kind());
    ensure_eq(18446744073709551616.0, over_both.as_decimal());

    jsonv::value under = jsonv::merge(jsonv::value(integer_min), jsonv::value(-1));
    ensure_eq(jsonv::kind::decimal, under.kind());
    ensure_eq(-9223372036854775808.0, under.as_decimal());

    jsonv::value under_both = jsonv::merge(jsonv::value(integer_min), jsonv::value(integer_min));
    ensure_eq(jsonv::kind::decimal, under_both.kind());
    ensure_eq(-18446744073709551616.0, under_both.as_decimal());
}

TEST(merge_integer_sum_at_limits)
{
    // A sum which lands exactly on a limit still fits
    jsonv::value to_max = jsonv::merge(jsonv::value(integer_max - 1), jsonv::value(1));
    ensure_eq(jsonv::kind::integer, to_max.kind());
    ensure_eq(integer_max, to_max.as_integer());

    jsonv::value to_min = jsonv::merge(jsonv::value(integer_min + 1), jsonv::value(-1));
    ensure_eq(jsonv::kind::integer, to_min.kind());
    ensure_eq(integer_min, to_min.as_integer());

    jsonv::value max_plus_zero = jsonv::merge(jsonv::value(integer_max), jsonv::value(0));
    ensure_eq(jsonv::kind::integer, max_plus_zero.kind());
    ensure_eq(integer_max, max_plus_zero.as_integer());

    jsonv::value min_plus_zero = jsonv::merge(jsonv::value(integer_min), jsonv::value(0));
    ensure_eq(jsonv::kind::integer, min_plus_zero.kind());
    ensure_eq(integer_min, min_plus_zero.as_integer());

    // Operands of opposite signs cannot overflow, whatever their size
    jsonv::value extremes = jsonv::merge(jsonv::value(integer_max), jsonv::value(integer_min));
    ensure_eq(jsonv::kind::integer, extremes.kind());
    ensure_eq(std::int64_t(-1), extremes.as_integer());

    jsonv::value extremes_reversed = jsonv::merge(jsonv::value(integer_min), jsonv::value(integer_max));
    ensure_eq(jsonv::kind::integer, extremes_reversed.kind());
    ensure_eq(std::int64_t(-1), extremes_reversed.as_integer());
}

TEST(merge_recursive_integer_overflow_member)
{
    jsonv::value result = jsonv::merge_recursive(jsonv::parse(R"({"n":9223372036854775807,"m":1})"),
                                                 jsonv::parse(R"({"n":1,"m":2})")
                                                );
    ensure_eq(jsonv::kind::decimal, result.at("n").kind());
    ensure_eq(9223372036854775808.0, result.at("n").as_decimal());
    // A member which fits is untouched by its neighbor's promotion
    ensure_eq(jsonv::kind::integer, result.at("m").kind());
    ensure_eq(std::int64_t(3), result.at("m").as_integer());
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

        // Only the merge is inside the `try`: an assertion failing in it would be caught and taken for the failure the
        // fixture expects.
        TMergeRules  rules;
        jsonv::value result;
        try
        {
            result = jsonv::merge_explicit(rules, jsonv::path(), a, b);
        }
        catch (...)
        {
            if (!expect_failure)
                throw;
            return;
        }

        ensure(!expect_failure);
        ensure_eq(expected, result);
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
