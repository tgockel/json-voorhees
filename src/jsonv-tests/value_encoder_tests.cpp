/// \file
/// Tests for \c jsonv::value_encoder, the \c encoder which builds a \c value from the tokens a \c writer gives it.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"
#include "allocation_counter.hpp"
#include "filesystem_util.hpp"

#include <jsonv/encode.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/value.hpp>
#include <jsonv/writer.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace jsonv_test
{

namespace
{

const char k_sample_json[] = R"({
  "a": [ 4, 5, 6, [7, 8, 9, {"something": 5, "else": 6}]],
  "b": "blah",
  "c": { "baz": ["bazar"], "cat": ["Eric", "Bob"] },
  "d": {},
  "e": [],
  "f": null,
  "g": [ true, false, -0.0, 1.5e300, "café" ]
})";

std::string read_file(const std::string& path)
{
    std::ifstream      in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return std::move(buffer).str();
}

/// Check that \a source comes back equal after a trip through a \c value_encoder, both by the value walk a \c writer
/// performs and by \c encoder::encode, which is the same walk.
void ensure_round_trips(const jsonv::value& source)
{
    jsonv::value_encoder sink;

    jsonv::writer(sink).write(source);
    ensure(std::move(sink).take() == source);

    sink.encode(source);
    ensure(std::move(sink).take() == source);
}

/// \a depth arrays nested one inside the next, the innermost empty: the value \c reader_from_value_deep_nesting
/// reads, built the same way. Moving rather than `jsonv::array({ ... })`, whose `initializer_list` elements are
/// `const` -- that spelling deep-copies the whole nesting once per level.
jsonv::value nested_arrays(std::size_t depth)
{
    jsonv::value nested = jsonv::array();
    for (std::size_t level = 1U; level < depth; ++level)
    {
        jsonv::value outer = jsonv::array();
        outer.push_back(std::move(nested));
        nested = std::move(outer);
    }
    return nested;
}

/// Check that \a built is what \c nested_arrays makes for \a depth, by walking down rather than by `==`, whose
/// comparison recurses once per level.
void ensure_nested_arrays(const jsonv::value& built, std::size_t depth)
{
    const jsonv::value* current = &built;
    for (std::size_t level = 1U; level < depth; ++level)
    {
        ensure(current->is_array());
        ensure_eq(std::size_t(1), current->size());
        current = &current->at(0);
    }
    ensure(current->is_array());
    ensure_eq(std::size_t(0), current->size());
}

}

TEST(value_encoder_tokens_build_value)
{
    jsonv::value_encoder sink;
    jsonv::writer        to(sink);
    to.object_begin()
        .key("a").array_begin().integer(1).integer(2).integer(3).array_end()
        .key("b").object_begin().key("x").string("taco").object_end()
        .key("c").integer(4)
        .key("d").decimal(2.5)
        .key("e").boolean(true)
        .key("f").null()
        .key("g").array_begin().array_end()
        .key("h").object_begin().object_end()
      .object_end();

    auto built = std::move(sink).take();
    ensure_eq(jsonv::parse(R"({"a":[1,2,3],"b":{"x":"taco"},"c":4,"d":2.5,"e":true,"f":null,"g":[],"h":{}})"), built);

    // An integer and a decimal compare equal when their values do, so the kinds are checked on their own.
    ensure(built.at("c").is_integer());
    ensure(built.at("d").is_decimal());
}

TEST(value_encoder_scalar_roots)
{
    // A scalar at depth zero is a whole document, and the writer accepts another once it is taken.
    jsonv::value_encoder sink;
    jsonv::writer        to(sink);

    to.null();
    ensure_eq(jsonv::value(), std::move(sink).take());
    to.boolean(true);
    ensure_eq(jsonv::value(true), std::move(sink).take());
    to.integer(-7);
    ensure_eq(jsonv::value(std::int64_t(-7)), std::move(sink).take());
    to.decimal(2.5);
    ensure_eq(jsonv::value(2.5), std::move(sink).take());
    to.string("taco");
    ensure_eq(jsonv::value("taco"), std::move(sink).take());
}

TEST(value_encoder_nested_empty_structures)
{
    jsonv::value_encoder sink;
    jsonv::writer(sink).array_begin().array_begin().array_end().object_begin().object_end().array_end();
    ensure_eq(jsonv::parse("[[],{}]"), std::move(sink).take());
}

TEST(value_encoder_repeated_key_keeps_last)
{
    jsonv::value_encoder sink;
    jsonv::writer        to(sink);
    to.object_begin()
        .key("a").array_begin().integer(1).object_begin().key("b").null().object_end().array_end()
        .key("a").integer(2)
        .key("c").string("x")
        .key("c").string("y")
      .object_end();

    auto built = std::move(sink).take();
    ensure_eq(jsonv::parse(R"({"a":2,"c":"y"})"), built);

    // Which is what `parse` keeps of the same document by default, so a tree built from tokens and a tree parsed from
    // the text those tokens spell select the same data.
    ensure_eq(jsonv::parse(R"({"a":[1,{"b":null}],"a":2,"c":"x","c":"y"})"), built);
}

TEST(value_encoder_second_root_replaces_first)
{
    jsonv::value_encoder sink;
    jsonv::writer        to(sink);
    to.object_begin().key("first").boolean(true).object_end();
    to.array_begin().integer(1).array_end();
    to.integer(2);
    ensure_eq(jsonv::value(std::int64_t(2)), std::move(sink).take());

    // As two calls to `encode` on one encoder leave the second.
    sink.encode(jsonv::object());
    sink.encode(jsonv::array({ 1 }));
    ensure_eq(jsonv::array({ 1 }), std::move(sink).take());
}

TEST(value_encoder_take_throws_while_open)
{
    jsonv::value_encoder sink;
    jsonv::writer        to(sink);
    to.array_begin().object_begin().key("a");
    ensure_throws(std::logic_error, std::move(sink).take());

    // The refusal lost nothing: the document finishes, and all of it is there.
    to.integer(1).object_end().array_end();
    ensure_eq(jsonv::parse(R"([{"a":1}])"), std::move(sink).take());
}

TEST(value_encoder_take_throws_when_nothing_written)
{
    jsonv::value_encoder sink;
    ensure_throws(std::logic_error, std::move(sink).take());

    // A document which has been taken is gone, so taking again is the same refusal -- and a written `null` is not
    // nothing.
    jsonv::writer(sink).null();
    ensure_eq(jsonv::value(), std::move(sink).take());
    ensure_throws(std::logic_error, std::move(sink).take());
}

TEST(value_encoder_decimal_special_values)
{
    // A `value` holds what JSON text cannot, so unlike `ostream_encoder`, nothing becomes `null` here.
    jsonv::value_encoder sink;
    jsonv::writer(sink)
        .array_begin()
            .decimal(std::numeric_limits<double>::quiet_NaN())
            .decimal(std::numeric_limits<double>::infinity())
            .decimal(-std::numeric_limits<double>::infinity())
            .decimal(-0.0)
        .array_end();

    auto built = std::move(sink).take();
    ensure_eq(std::size_t(4), built.size());
    ensure(built.at(0).is_decimal());
    ensure(std::isnan(built.at(0).as_decimal()));
    ensure_eq(std::numeric_limits<double>::infinity(), built.at(1).as_decimal());
    ensure_eq(-std::numeric_limits<double>::infinity(), built.at(2).as_decimal());
    ensure(built.at(3).is_decimal());
    ensure(std::signbit(built.at(3).as_decimal()));
}

TEST(value_encoder_round_trips_sample)
{
    ensure_round_trips(jsonv::parse(k_sample_json));
}

TEST(value_encoder_round_trips_corpus)
{
    for (const char* name : { "blns.json", "paths.json", "canada.json", "citm_catalog.json", "generated.json" })
    {
        auto src = read_file(test_path(name));
        ensure(!src.empty());
        ensure_round_trips(jsonv::parse(src));
    }
}

TEST(value_encoder_round_trips_fuzz_corpus)
{
    // `test_path` is rooted at `src/jsonv-tests/data`, and the fuzz seeds are a sibling of that tree.
    auto root = test_path("../../jsonv-fuzz/corpus");

    std::size_t compared = 0U;
    recursive_directory_for_each(root, ".json", [&](const std::string& path)
        {
            auto src = read_file(path);
            ensure(!src.empty());

            jsonv::value parsed;
            try
            {
                parsed = jsonv::parse(src);
            }
            catch (const std::exception&)
            {
                // A seed which does not parse, or holds a number no `double` can, has no value to compare.
                return;
            }

            // Unlike the text encoders, nothing here can lose a string which is not valid UTF-8: the bytes are copied
            // as they are, so every seed which parses is a baseline.
            ensure_round_trips(parsed);
            ++compared;
        });

    ensure_ge(compared, std::size_t(8U));
}

/// The frame stack is a `std::vector`, so depth costs heap instead of call frames.
TEST(value_encoder_deep_nesting_tokens)
{
    constexpr std::size_t depth = 4096U;

    jsonv::value_encoder sink;
    jsonv::writer        to(sink);
    for (std::size_t idx = 0U; idx < depth; ++idx)
        to.array_begin();
    for (std::size_t idx = 0U; idx < depth; ++idx)
        to.array_end();

    ensure_nested_arrays(std::move(sink).take(), depth);
}

/// The value walk behind `writer::write` recurses, as `encode` always has; this is the deepest it is asked to go.
TEST(value_encoder_deep_nesting_write_tree)
{
    constexpr std::size_t depth = 4096U;
    const auto            source = nested_arrays(depth);

    jsonv::value_encoder sink;
    jsonv::writer(sink).write(source);
    ensure(std::move(sink).take() == source);
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

/// Writing a string copies its text once, which is what `value(std::string_view)` costs, and the encoder adds nothing
/// of its own; a member copies its key once as well, into the buffer the encoder keeps it in, from which it is moved
/// into the map node rather than copied again. Copying text past the small-string buffer is an allocation and
/// copying text inside it is not, while whatever else is allocated along the way -- the node, the map node, and under
/// MSVC's debug iterator checks a proxy per `std::string` constructed or moved -- is allocated whatever the length. So
/// the copies are the difference between writing long text and short text, and an extra copy would show as one more.
TEST(value_encoder_string_costs_one_copy)
{
    const std::string long_text(64, 'x');
    const std::string short_text(1, 'x');

    jsonv::value_encoder sink;
    jsonv::writer        to(sink);

    // A root string: one copy, the text itself.
    auto root_cost = [&](const std::string& text)
        {
            allocation_counter allocations;
            to.string(text);
            const std::size_t cost = allocations.count();
            ensure_eq(jsonv::value(text), std::move(sink).take());
            return cost;
        };
    ensure_eq(root_cost(short_text) + 1U, root_cost(long_text));

    // A member: two copies, the key into the encoder's buffer and the text. The keys differ so that each member is a
    // new node, and the first member grows the writer's own key buffer, which the writer keeps, so the two measured
    // are the steady state.
    const std::string first_key = long_text + "kk";
    const std::string short_key = "k";
    const std::string long_key  = long_text + "k";
    auto member_cost = [&](const std::string& key, const std::string& text)
        {
            allocation_counter allocations;
            to.key(key).string(text);
            return allocations.count();
        };
    to.object_begin();
    to.key(first_key).string(long_text);
    const std::size_t short_member = member_cost(short_key, short_text);
    const std::size_t long_member  = member_cost(long_key, long_text);
    to.object_end();
    ensure_eq(short_member + 2U, long_member);
    ensure_eq(jsonv::object({ { first_key, long_text }, { short_key, short_text }, { long_key, long_text } }),
              std::move(sink).take());
}

#endif

}
