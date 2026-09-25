/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv-tests/test.hpp>

#include <jsonv-tests/filesystem_util.hpp>

#include <jsonv/ast.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/parse_index.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/value.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef JSONV_READER_TESTS_LOG_ENABLED
#   define JSONV_READER_TESTS_LOG_ENABLED 0
#endif

#if JSONV_READER_TESTS_LOG_ENABLED
#   define JSONV_READER_TESTS_LOG(...) std::cout << __VA_ARGS__
#else
#   define JSONV_READER_TESTS_LOG(...) do {} while (false)
#endif

namespace jsonv_tests
{

namespace
{

struct walk_info
{
    jsonv::ast_node_type type;
    jsonv::path          path;
    std::size_t          next_key_idx;
    std::size_t          next_struct_idx;

    walk_info(jsonv::ast_node_type type,
              std::string_view   path,
              std::size_t          next_key_idx,
              std::size_t          next_struct_idx) :
            type(type),
            path(jsonv::path::create(path)),
            next_key_idx(next_key_idx),
            next_struct_idx(next_struct_idx)
    { }
};

struct example_data_type
{
    std::string_view     name;
    std::string_view     source;
    std::vector<walk_info> expected;
};

/// Which \c reader the walk is against. Every example is walked both ways: the same document reached through the
/// parser and through an in-memory \c value has to produce the same node sequence, or an extractor written against
/// one source would silently misbehave on the other.
enum class walk_source
{
    text,   //!< `reader(std::string_view)`, reading the example's source text
    value,  //!< `reader::from_value`, reading the example parsed into a `value` first
};

}

static std::string to_string(walk_source source)
{
    return source == walk_source::value ? "value" : "text";
}

static jsonv::reader make_reader(std::string_view source, walk_source from)
{
    if (from == walk_source::value)
        return jsonv::reader::from_value(jsonv::parse(source));
    else
        return jsonv::reader(source);
}

/// A `value` holds *decoded* string bytes, so a value-sourced reader reports every string and key as canonical no
/// matter how the source document spelled it. Nothing else about the walk differs, which is the point.
static jsonv::ast_node_type expected_type(jsonv::ast_node_type type, walk_source from)
{
    if (from == walk_source::text)
        return type;

    switch (type)
    {
    case jsonv::ast_node_type::string_escaped: return jsonv::ast_node_type::string_canonical;
    case jsonv::ast_node_type::key_escaped:    return jsonv::ast_node_type::key_canonical;
    default:                                   return type;
    }
}

template <typename FShouldJumpNextKey, typename FShouldJumpNextStruct>
static void walk_expecting(jsonv::reader&                reader,
                           const std::vector<walk_info>& expected,
                           FShouldJumpNextKey&&          should_jump_next_key,
                           FShouldJumpNextStruct&&       should_jump_next_struct,
                           walk_source                   from = walk_source::text
                          )
{

    ensure_eq(jsonv::ast_node_type::document_start, reader.current().type());
    ensure(reader.next_token());

    for (std::size_t idx = 0U; idx < expected.size(); /* inline */)
    {
        auto current = reader.current();

        ensure_eq(expected_type(expected[idx].type, from), current.type());
        ensure_eq(expected[idx].path, reader.current_path());

        if (auto type = current.type();
            (   type == jsonv::ast_node_type::object_begin
             || type == jsonv::ast_node_type::key_canonical
             || type == jsonv::ast_node_type::key_escaped
            )
            && should_jump_next_key(current, idx)
           )
        {
            ensure(reader.next_key());

            JSONV_READER_TESTS_LOG
            (
                std::endl
                << " - next_key: from " << expected[idx].path << " @ [" << idx << "] "
                << "to " << expected[expected[idx].next_key_idx].path << " @ [" << expected[idx].next_key_idx << "]"
            );
            idx = expected[idx].next_key_idx;
        }
        else if (should_jump_next_struct(current, idx))
        {
            JSONV_READER_TESTS_LOG(std::endl << " - next_struct: from " << expected[idx].path << " @ [" << idx << "] ");
            ensure(reader.next_structure());
            JSONV_READER_TESTS_LOG("to [" << expected[idx].next_struct_idx << "]");
            idx = expected[idx].next_struct_idx;
        }
        else
        {
            ensure(reader.next_token());
            ++idx;
        }
    }

    ensure_eq(jsonv::ast_node_type::document_end, reader.current().type());
    ensure(!reader.next_token());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Examples                                                                                                           //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char example_basic_source[] =
    R"(
    {
        "a": [ 1, 2, 3 ],
        "b": { "key": "value" },
        "c": 4
    }
    )";

static std::vector<walk_info> example_basic_walk_info =
{ //  node type,                              path,     key, struct,       listing
    { jsonv::ast_node_type::object_begin,     ".",       1U, 15U }, //  0: {
    { jsonv::ast_node_type::key_canonical,    ".a",      7U, 15U }, //  1:   "a":
    { jsonv::ast_node_type::array_begin,      ".a",      0U,  7U }, //  2:     [
    { jsonv::ast_node_type::integer,          ".a[0]",   0U,  7U }, //  3:       1,
    { jsonv::ast_node_type::integer,          ".a[1]",   0U,  7U }, //  4:       2,
    { jsonv::ast_node_type::integer,          ".a[2]",   0U,  7U }, //  5:       3
    { jsonv::ast_node_type::array_end,        ".a",      0U,  7U }, //  6:     ],
    { jsonv::ast_node_type::key_canonical,    ".b",     12U, 15U }, //  7:   "b":
    { jsonv::ast_node_type::object_begin,     ".b",      9U, 12U }, //  8:     {
    { jsonv::ast_node_type::key_canonical,    ".b.key", 11U, 12U }, //  9:       "key":
    { jsonv::ast_node_type::string_canonical, ".b.key",  0U, 12U }, // 10:         "value"
    { jsonv::ast_node_type::object_end,       ".b",      0U, 12U }, // 11:     },
    { jsonv::ast_node_type::key_canonical,    ".c",     14U, 15U }, // 12:   "c":
    { jsonv::ast_node_type::integer,          ".c",      0U, 15U }, // 13:     4
    { jsonv::ast_node_type::object_end,       ".",       0U, 15U }, // 14: }
};

static const char example_nestings_source[] =
    R"(
    {
        "cat": [ {}, {}, { "mouse": [{}] }, "\n" ],
        "dog": { "food": [[[[], [-5]], 3.14]] }
    }
    )";

static std::vector<walk_info> example_nestings_walk_info =
{ //  node type,                                path,                    key, struct,        listing
    { jsonv::ast_node_type::object_begin,       ".",                      1U,  33U }, //  0: {
    { jsonv::ast_node_type::key_canonical,      ".cat",                  16U,  33U }, //  1:   "cat":
    { jsonv::ast_node_type::array_begin,        ".cat",                   0U,  16U }, //  2:     [
    { jsonv::ast_node_type::object_begin,       ".cat[0]",                4U,   5U }, //  3:       {
    { jsonv::ast_node_type::object_end,         ".cat[0]",                0U,   5U }, //  4:       },
    { jsonv::ast_node_type::object_begin,       ".cat[1]",                6U,   7U }, //  5:       {
    { jsonv::ast_node_type::object_end,         ".cat[1]",                0U,   7U }, //  6:       },
    { jsonv::ast_node_type::object_begin,       ".cat[2]",                8U,  14U }, //  7:       {
    { jsonv::ast_node_type::key_canonical,      ".cat[2].mouse",         13U,  14U }, //  8:         "mouse":
    { jsonv::ast_node_type::array_begin,        ".cat[2].mouse",          0U,  13U }, //  9:           [
    { jsonv::ast_node_type::object_begin,       ".cat[2].mouse[0]",      11U,  12U }, // 10:             {
    { jsonv::ast_node_type::object_end,         ".cat[2].mouse[0]",       0U,  12U }, // 11:             }
    { jsonv::ast_node_type::array_end,          ".cat[2].mouse",          0U,  13U }, // 12:           ]
    { jsonv::ast_node_type::object_end,         ".cat[2]",                0U,  14U }, // 13:       },
    { jsonv::ast_node_type::string_escaped,     ".cat[3]",                0U,  16U }, // 14:       "\n"
    { jsonv::ast_node_type::array_end,          ".cat",                   0U,  16U }, // 15:     ],
    { jsonv::ast_node_type::key_canonical,      ".dog",                  32U,  33U }, // 16:   "dog":
    { jsonv::ast_node_type::object_begin,       ".dog",                  18U,  32U }, // 17:     {
    { jsonv::ast_node_type::key_canonical,      ".dog.food",             31U,  32U }, // 18:       "food":
    { jsonv::ast_node_type::array_begin,        ".dog.food",              0U,  31U }, // 19:         [
    { jsonv::ast_node_type::array_begin,        ".dog.food[0]",           0U,  30U }, // 20:           [
    { jsonv::ast_node_type::array_begin,        ".dog.food[0][0]",        0U,  28U }, // 21:             [
    { jsonv::ast_node_type::array_begin,        ".dog.food[0][0][0]",     0U,  24U }, // 22:               [
    { jsonv::ast_node_type::array_end,          ".dog.food[0][0][0]",     0U,  24U }, // 23:               ],
    { jsonv::ast_node_type::array_begin,        ".dog.food[0][0][1]",     0U,  27U }, // 24:               [
    { jsonv::ast_node_type::integer,            ".dog.food[0][0][1][0]",  0U,  27U }, // 25:                 -5
    { jsonv::ast_node_type::array_end,          ".dog.food[0][0][1]",     0U,  27U }, // 26:               ]
    { jsonv::ast_node_type::array_end,          ".dog.food[0][0]",        0U,  28U }, // 27:             ],
    { jsonv::ast_node_type::decimal,            ".dog.food[0][1]",        0U,  30U }, // 28:             3.14
    { jsonv::ast_node_type::array_end,          ".dog.food[0]",           0U,  30U }, // 29:           ]
    { jsonv::ast_node_type::array_end,          ".dog.food",              0U,  31U }, // 30:         ]
    { jsonv::ast_node_type::object_end,         ".dog",                   0U,  32U }, // 31:     }
    { jsonv::ast_node_type::object_end,         ".",                      0U,  33U }, // 32: }
};

static const char example_array_source[] = R"([ "a", "b", "c" ])";

static const std::vector<walk_info> example_array_walk_info =
{ //  node type,                              path,     key, struct,       listing
    { jsonv::ast_node_type::array_begin,      ".",       0U,  5U },  // 0: [
    { jsonv::ast_node_type::string_canonical, "[0]",     0U,  5U },  // 1:   "a",
    { jsonv::ast_node_type::string_canonical, "[1]",     0U,  5U },  // 2:   "b",
    { jsonv::ast_node_type::string_canonical, "[2]",     0U,  5U },  // 3:   "c"
    { jsonv::ast_node_type::array_end,        ".",       0U,  5U },  // 4: ]
};

static const char example_string_source[] = R"("taco\nstand")";

static const std::vector<walk_info> example_string_walk_info =
{
    { jsonv::ast_node_type::string_escaped, ".", 0U, 1U }
};

static const std::vector<example_data_type> examples =
{
    { "basic",    example_basic_source,     example_basic_walk_info },
    { "nestings", example_nestings_source,  example_nestings_walk_info },
    { "array",    example_array_source,     example_array_walk_info },
    { "string",   example_string_source,    example_string_walk_info },
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Unit Tests                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// These tests walk through the entire object without doing any jumps.
class reader_example_walkthrough_no_jump_test final :
        public jsonv_test::unit_test
{
public:
    reader_example_walkthrough_no_jump_test(example_data_type data, walk_source from) :
            jsonv_test::unit_test(std::string("reader_example_walkthrough_no_jump/") + std::string(data.name)
                                  + "/" + to_string(from)),
            _data(std::move(data)),
            _from(from)
    { }

    static std::vector<std::unique_ptr<jsonv_test::unit_test>> make_tests(const example_data_type& data)
    {
        std::vector<std::unique_ptr<jsonv_test::unit_test>> out;
        for (auto from : { walk_source::text, walk_source::value })
            out.emplace_back(std::make_unique<reader_example_walkthrough_no_jump_test>(data, from));
        return out;
    }

protected:
    virtual void run_impl() override
    {
        auto reader = make_reader(_data.source, _from);
        walk_expecting(reader,
                       _data.expected,
                       [](auto&&...) { return false; },
                       [](auto&&...) { return false; },
                       _from
                      );
    }

private:
    example_data_type _data;
    walk_source       _from;
};

static void walk_expecting_next_struct_at_index(std::string_view            src,
                                                const std::vector<walk_info>& expected,
                                                std::size_t                   jump_from_idx,
                                                walk_source                   from
                                               )
{
    JSONV_READER_TESTS_LOG(std::endl << "to " << jump_from_idx << "...");
    auto reader = make_reader(src, from);
    walk_expecting(reader,
                   expected,
                   [](auto&&...) { return false; },
                   [&](auto&&, auto idx) { return idx == jump_from_idx; },
                   from
                  );
}

class reader_example_walkthrough_next_struct_at_index final :
        public jsonv_test::unit_test
{
public:
    reader_example_walkthrough_next_struct_at_index(example_data_type data,
                                                   std::size_t       next_struct_at_idx,
                                                   walk_source       from
                                                  ) :
            jsonv_test::unit_test(std::string("reader_example_walkthrough_next_struct_at_index/")
                                  + std::string(data.name) + "/" + std::to_string(next_struct_at_idx)
                                  + "/" + to_string(from)),
            _data(std::move(data)),
            _next_struct_at_idx(next_struct_at_idx),
            _from(from)
    { }

    static std::vector<std::unique_ptr<jsonv_test::unit_test>> make_tests(const example_data_type& data)
    {
        std::vector<std::unique_ptr<jsonv_test::unit_test>> out;
        out.reserve(data.expected.size() * 2U);
        for (std::size_t idx = 0U; idx < data.expected.size(); ++idx)
            for (auto from : { walk_source::text, walk_source::value })
                out.emplace_back(std::make_unique<reader_example_walkthrough_next_struct_at_index>(data, idx, from));
        return out;
    }

protected:
    virtual void run_impl() override
    {
        walk_expecting_next_struct_at_index(_data.source, _data.expected, _next_struct_at_idx, _from);
    }

private:
    example_data_type _data;
    std::size_t       _next_struct_at_idx;
    walk_source       _from;
};

static void walk_expecting_next_key_at_index(std::string_view            src,
                                             const std::vector<walk_info>& expected,
                                             std::size_t                   jump_from_idx,
                                             walk_source                   from
                                            )
{
    JSONV_READER_TESTS_LOG(std::endl << "to " << jump_from_idx << "...");
    auto reader = make_reader(src, from);
    walk_expecting(reader,
                   expected,
                   [&](auto&&, auto idx) { return idx == jump_from_idx; },
                   [](auto&&...) { return false; },
                   from
                  );
}

class reader_example_walkthrough_next_key_at_index final :
        public jsonv_test::unit_test
{
public:
    reader_example_walkthrough_next_key_at_index(example_data_type data,
                                                std::size_t       next_key_at_idx,
                                                walk_source       from
                                               ) :
            jsonv_test::unit_test(std::string("reader_example_walkthrough_next_key_at_index/")
                                  + std::string(data.name) + "/" + std::to_string(next_key_at_idx)
                                  + "/" + to_string(from)),
            _data(std::move(data)),
            _next_key_at_idx(next_key_at_idx),
            _from(from)
    { }

    static std::vector<std::unique_ptr<jsonv_test::unit_test>> make_tests(const example_data_type& data)
    {
        std::vector<std::unique_ptr<jsonv_test::unit_test>> out;
        out.reserve(data.expected.size() * 2U);
        for (std::size_t idx = 0U; idx < data.expected.size(); ++idx)
        {
            if (data.expected[idx].next_key_idx != 0U)
                for (auto from : { walk_source::text, walk_source::value })
                    out.emplace_back(std::make_unique<reader_example_walkthrough_next_key_at_index>(data, idx, from));
        }
        return out;
    }

protected:
    virtual void run_impl() override
    {
        walk_expecting_next_key_at_index(_data.source, _data.expected, _next_key_at_idx, _from);
    }

private:
    example_data_type _data;
    std::size_t       _next_key_at_idx;
    walk_source       _from;
};

/// Walk through \a source, randomly calling \c next_key with \a next_key_probability (when current position is a key
/// type) and \c next_struct with \a next_struct_probability. The random number generator is seeded with \a rng_seed so
/// behavior is deterministic.
static void walk_random_expecting(std::string_view            source,
                                  const std::vector<walk_info>& expected,
                                  double                        next_key_probability    = 0.0,
                                  double                        next_struct_probability = 0.0,
                                  std::size_t                   rng_seed                = 0U,
                                  walk_source                   from                    = walk_source::text
                                 )
{
    if ((next_key_probability > 0.0 || next_struct_probability > 0.0) && rng_seed == 0U)
    {
        rng_seed = std::random_device{}();
        JSONV_READER_TESTS_LOG("seed=" << rng_seed);
    }

    std::minstd_rand            rng(rng_seed);
    std::bernoulli_distribution next_key_dist(next_key_probability);
    std::bernoulli_distribution next_struct_dist(next_struct_probability);

    auto reader = make_reader(source, from);
    return walk_expecting(reader,
                          expected,
                          [&](auto&&...) { return next_key_dist(rng); },
                          [&](auto&&...) { return next_struct_dist(rng); },
                          from
                         );
}

class reader_example_walkthrough_randomly final :
        public jsonv_test::unit_test
{
public:
    reader_example_walkthrough_randomly(example_data_type data,
                                        double            next_struct_probability,
                                        double            next_key_probability,
                                        std::size_t       seed,
                                        walk_source       from
                                       ) :
            jsonv_test::unit_test(std::string("reader_example_walkthrough_randomly/")
                                  + std::string(data.name)
                                  + "/P(next_struct=" + std::to_string(next_struct_probability)
                                       + ",next_key=" + std::to_string(next_key_probability) + ")/"
                                  + std::to_string(seed) + "/" + to_string(from)),
            _data(std::move(data)),
            _next_struct_probability(next_struct_probability),
            _next_key_probability(next_key_probability),
            _seed(seed),
            _from(from)
    { }

    static std::vector<std::unique_ptr<jsonv_test::unit_test>> make_tests(const example_data_type& data)
    {
        if (data.expected.size() == 1U)
            return {};

        std::vector<std::unique_ptr<jsonv_test::unit_test>> out;

        std::mt19937_64                            rng(std::random_device{}());
        std::uniform_int_distribution<std::size_t> dist;

        for (double next_struct_p : { 0.0, 0.1, 0.2, 0.4, 0.8 })
        {
            for (double next_key_p : { 0.0, 0.1, 0.2, 0.4, 0.8 })
            {
                for (std::size_t seed_idx = 0U; seed_idx < 10U; ++seed_idx)
                {
                    auto seed = dist(rng);
                    for (auto from : { walk_source::text, walk_source::value })
                    {
                        out.emplace_back(std::make_unique<reader_example_walkthrough_randomly>(data,
                                                                                               next_struct_p,
                                                                                               next_key_p,
                                                                                               seed,
                                                                                               from
                                                                                              )
                                        );
                    }
                }
            }
        }

        // Throw in some random distributions for good measure
        std::uniform_real_distribution next_struct_dist;
        std::uniform_real_distribution next_key_dist;
        for (std::size_t count = 0U; count < data.expected.size(); ++count)
        {
            auto next_struct_p = next_struct_dist(rng);
            auto next_key_p    = next_key_dist(rng);
            auto seed          = dist(rng);
            for (auto from : { walk_source::text, walk_source::value })
            {
                out.emplace_back(std::make_unique<reader_example_walkthrough_randomly>(data,
                                                                                       next_struct_p,
                                                                                       next_key_p,
                                                                                       seed,
                                                                                       from
                                                                                      )
                                );
            }
        }

        return out;
    }

protected:
    virtual void run_impl() override
    {
        walk_random_expecting(_data.source,
                              _data.expected,
                              _next_key_probability,
                              _next_struct_probability,
                              _seed,
                              _from
                             );
    }

private:
    example_data_type _data;
    double            _next_struct_probability;
    double            _next_key_probability;
    std::size_t       _seed;
    walk_source       _from;
};

static std::vector<std::vector<std::unique_ptr<jsonv_test::unit_test>>> create_all_tests()
{
    std::vector<std::vector<std::unique_ptr<jsonv_test::unit_test>>> out;
    for (const auto& example : examples)
    {
        out.emplace_back(reader_example_walkthrough_no_jump_test::make_tests(example));
        out.emplace_back(reader_example_walkthrough_next_struct_at_index::make_tests(example));
        out.emplace_back(reader_example_walkthrough_next_key_at_index::make_tests(example));
        out.emplace_back(reader_example_walkthrough_randomly::make_tests(example));
    }
    return out;
}

static std::vector<std::vector<std::unique_ptr<jsonv_test::unit_test>>> all_tests = create_all_tests();


/// Walk to node \a index, then report where `next_value` lands, as an index into the token stream.
static std::size_t next_value_from(std::string_view src, std::size_t index)
{
    jsonv::reader reader(src);
    for (std::size_t n = 0U; n < index; ++n)
        ensure(reader.next_token());

    if (!reader.next_value())
        return std::size_t(-1);

    // Find where we ended up by counting from the start again.
    jsonv::reader counter(src);
    for (std::size_t n = 0U; ; ++n)
    {
        if (counter.current().type()       == reader.current().type()
            && counter.current_path()      == reader.current_path()
            && counter.current().token_raw().data() == reader.current().token_raw().data()
           )
        {
            return n;
        }

        if (!counter.next_token())
            return std::size_t(-2);
    }
}

/// `next_value` steps over exactly the value the reader is on. Contrast `next_structure`, which leaves the structure
/// the reader is *inside* -- on a scalar member those two differ, and confusing them silently consumes the rest of the
/// enclosing object.
TEST(reader_next_value_steps_over_one_value)
{
    static const char src[] = R"({ "a": [ 1, 2, 3 ], "b": { "key": "value" }, "c": 4 })";

    // Token stream, counting the document start the reader begins on:
    //   0 ^   1 {   2 "a"   3 [   4 1   5 2   6 3   7 ]   8 "b"   9 {   10 "key"   11 "value"
    //   12 }   13 "c"   14 4   15 }   16 $

    // Over a structure: the `[` at 3 lands on the key "b" at 8, and the `{` at 9 on the key "c" at 13.
    ensure_eq(8U,  next_value_from(src, 3U));
    ensure_eq(13U, next_value_from(src, 9U));

    // Over a scalar it is a single step: `1` at 4 -> `2` at 5.
    ensure_eq(5U, next_value_from(src, 4U));

    // And the case the two primitives disagree on: the scalar `4` at 14 goes to the enclosing `}` at 15, where
    // `next_structure` would leave the object altogether.
    ensure_eq(15U, next_value_from(src, 14U));

    {
        jsonv::reader reader(src);
        for (std::size_t n = 0U; n < 14U; ++n)
            ensure(reader.next_token());
        ensure(reader.next_structure());
        ensure_eq(jsonv::ast_node_type::document_end, reader.current().type());
    }
}

/// The tape records where each structure ends, so `impl_parse_index` overrides the depth-counting walk. Both have to
/// agree, for every position in the document.
TEST(reader_next_value_agrees_with_counting_the_way_out)
{
    for (const auto& example : examples)
    {
        jsonv::reader probe(example.source);

        for (std::size_t index = 0U; probe.good(); ++index, static_cast<void>(probe.next_token()))
        {
            auto type = probe.current().type();
            if (type != jsonv::ast_node_type::object_begin && type != jsonv::ast_node_type::array_begin)
                continue;

            // Count the way out by hand, exactly as the base implementation would.
            jsonv::reader counted(example.source);
            for (std::size_t n = 0U; n < index; ++n)
                ensure(counted.next_token());

            std::size_t depth = 0U;
            do
            {
                auto tok = counted.current().type();
                if (tok == jsonv::ast_node_type::object_begin || tok == jsonv::ast_node_type::array_begin)
                {
                    ++depth;
                }
                else if (tok == jsonv::ast_node_type::object_end || tok == jsonv::ast_node_type::array_end)
                {
                    --depth;
                    if (depth == 0U)
                        break;
                }
            } while (counted.next_token());
            ensure(counted.next_token());

            jsonv::reader jumped(example.source);
            for (std::size_t n = 0U; n < index; ++n)
                ensure(jumped.next_token());
            ensure(jumped.next_value());

            ensure_eq(counted.current().type(), jumped.current().type());
            ensure_eq(counted.current_path(),   jumped.current_path());
            ensure_eq(static_cast<const void*>(counted.current().token_raw().data()),
                      static_cast<const void*>(jumped.current().token_raw().data())
                     );
        }
    }
}

/// \see https://github.com/tgockel/json-voorhees/issues/213
TEST(reader_next_structure_on_exhausted_reader)
{
    jsonv::reader reader("[1, 2]");
    while (reader.next_token())
    { }

    // The reader is exhausted, so there is no current node to inspect. This must report failure instead of letting
    // `current`'s exception escape the `noexcept` boundary.
    ensure(!reader.next_structure());
}

/// \see https://github.com/tgockel/json-voorhees/issues/213
TEST(reader_next_key_on_non_key_node_throws)
{
    jsonv::reader reader("[1, 2]");
    ensure(reader.next_token());    // document_start -> array_begin
    ensure_throws(std::invalid_argument, reader.next_key());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// expect and current_as                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(ast_node_expect_single_type)
{
    jsonv::ast_node node(std::in_place_type<jsonv::ast_node::literal_true>, "true");

    ensure(node.expect(jsonv::ast_node_type::literal_true).has_value());

    auto res = node.expect(jsonv::ast_node_type::literal_false);
    ensure(!res.has_value());
    ensure_eq(jsonv::ast_node_type::literal_true, res.error());
}

/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(ast_node_expect_type_list)
{
    jsonv::ast_node node(std::in_place_type<jsonv::ast_node::literal_null>, "null");

    ensure(node.expect({ jsonv::ast_node_type::literal_true, jsonv::ast_node_type::literal_null }).has_value());

    auto res = node.expect({ jsonv::ast_node_type::literal_true, jsonv::ast_node_type::literal_false });
    ensure(!res.has_value());
    ensure_eq(jsonv::ast_node_type::literal_null, res.error());
}

/// A one-element list takes a separate path which delegates to the single-type overload -- check it agrees. Note that
/// a braced list always binds to the `initializer_list` overload, even with one element.
///
/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(reader_expect_type_list_of_one)
{
    jsonv::reader reader("[1]");
    ensure(reader.next_token());    // document_start -> array_begin

    ensure(reader.expect({ jsonv::ast_node_type::array_begin }).has_value());

    auto res = reader.expect({ jsonv::ast_node_type::integer });
    ensure(!res.has_value());
    ensure_eq(jsonv::ast_node_type::array_begin, res.error());
}

/// Expecting nothing at all is a mistake in the caller, not a property of the JSON, so it throws rather than travelling
/// through the error channel extractors handle.
///
/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(reader_expect_empty_type_list_throws)
{
    jsonv::reader reader("5");
    ensure(reader.next_token());    // document_start -> integer
    ensure_throws(std::invalid_argument, reader.expect({}));
}

/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(reader_expect_forwards_to_current)
{
    jsonv::reader reader(R"({ "a": 1 })");
    ensure(reader.next_token());    // document_start -> object_begin

    ensure(reader.expect(jsonv::ast_node_type::object_begin).has_value());

    auto res = reader.expect(jsonv::ast_node_type::array_begin);
    ensure(!res.has_value());
    ensure_eq(jsonv::ast_node_type::object_begin, res.error());
}

/// `current_as` is a template, so it is only checked when something instantiates it. Nothing did, which is how it came
/// to be `const` while calling a non-`const` `expect` -- a hard error for every node type. Instantiate it here for a
/// fixed-size token and a dynamic-size one so the signature cannot rot again.
///
/// \see https://github.com/tgockel/json-voorhees/issues/222
TEST(reader_current_as_instantiates)
{
    jsonv::reader reader(R"([ 1, "taco" ])");
    ensure(reader.next_token());    // document_start -> array_begin

    // Fixed-size token.
    auto arr = reader.current_as<jsonv::ast_node::array_begin>();
    ensure(arr.has_value());
    ensure_eq(2U, arr->element_count());

    // Dynamic-size token.
    ensure(reader.next_token());
    auto num = reader.current_as<jsonv::ast_node::integer>();
    ensure(num.has_value());
    ensure_eq(std::int64_t(1), num->value());

    // Dynamic-size token carrying a string.
    ensure(reader.next_token());
    auto str = reader.current_as<jsonv::ast_node::string_canonical>();
    ensure(str.has_value());
    ensure_eq("taco", str->value());
}

/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(reader_current_as_mismatch_reports_the_type_found)
{
    jsonv::reader reader("5");
    ensure(reader.next_token());    // document_start -> integer

    auto res = reader.current_as<jsonv::ast_node::string_canonical>();
    ensure(!res.has_value());
    ensure_eq(jsonv::ast_node_type::integer, res.error());
}

/// Both are `const`, so they must work through a `const` reference. `current_as` being `const` while `expect` was not
/// is what made it impossible to instantiate.
///
/// \see https://github.com/tgockel/json-voorhees/issues/222
TEST(reader_expect_and_current_as_on_const_reader)
{
    jsonv::reader reader("5");
    ensure(reader.next_token());    // document_start -> integer

    const jsonv::reader& const_reader = reader;
    ensure(const_reader.expect(jsonv::ast_node_type::integer).has_value());
    ensure(const_reader.expect({ jsonv::ast_node_type::integer, jsonv::ast_node_type::decimal }).has_value());

    auto res = const_reader.current_as<jsonv::ast_node::integer>();
    ensure(res.has_value());
    ensure_eq(std::int64_t(5), res->value());
}

/// The header claimed `std::invalid_argument` for a reader which is not `good`, but that case comes out of
/// `reader::impl::current` as a `std::logic_error`. `std::invalid_argument` is only the moved-from case, which is
/// pinned by `reader_moved_from_source_throws_invalid_argument` below.
///
/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(reader_expect_on_exhausted_reader_throws_logic_error)
{
    jsonv::reader reader("5");
    while (reader.next_token())
        ;

    ensure(!reader.good());
    ensure_throws(std::logic_error, reader.expect(jsonv::ast_node_type::integer));
    ensure_throws(std::logic_error, reader.expect({ jsonv::ast_node_type::integer }));
    ensure_throws(std::logic_error, reader.current_as<jsonv::ast_node::integer>());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Moving                                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// These are as much compiled as run. This file sees only the public headers, where `reader::impl` is incomplete, so a
// `reader` whose move operations are inline `= default` does not compile here at all -- the deleter needs a complete
// type. That is the whole of #240, and any of the four below is enough to catch it coming back.

/// \see https://github.com/tgockel/json-voorhees/issues/240
TEST(reader_move_construct_keeps_position)
{
    jsonv::reader src(R"({ "a": [ 1, 2, 3 ] })");
    ensure(src.next_token());   // document_start -> {
    ensure(src.next_token());   // { -> "a"
    ensure(src.next_token());   // "a" -> [
    ensure(src.next_token());   // [ -> 1

    jsonv::reader dst(std::move(src));

    ensure(dst.good());
    ensure_eq(jsonv::ast_node_type::integer, dst.current().type());
    ensure_eq(jsonv::path::create(".a[0]"), dst.current_path());
    ensure_eq(std::int64_t(1), dst.current().as<jsonv::ast_node::integer>().value());

    // ...and the walk carries on from where the source left off.
    ensure(dst.next_token());
    ensure_eq(std::int64_t(2), dst.current().as<jsonv::ast_node::integer>().value());
}

/// The destination's old implementation has to be destroyed, which is the half that needs a complete type. Assigning
/// over a reader part-way through a different document is what makes a leak or a double free show up under ASan.
///
/// \see https://github.com/tgockel/json-voorhees/issues/240
TEST(reader_move_assign_over_populated_destination)
{
    jsonv::reader src(R"({ "a": "taco" })");
    ensure(src.next_token());   // document_start -> {
    ensure(src.next_token());   // { -> "a"
    ensure(src.next_token());   // "a" -> "taco"

    jsonv::reader dst(R"([ 1, 2, 3 ])");
    ensure(dst.next_token());   // document_start -> [
    ensure(dst.next_token());   // [ -> 1

    dst = std::move(src);

    ensure(dst.good());
    ensure_eq(jsonv::ast_node_type::string_canonical, dst.current().type());
    ensure_eq(jsonv::path::create(".a"), dst.current_path());
    ensure_eq("taco", dst.current().as<jsonv::ast_node::string_canonical>().value());

    ensure(dst.next_token());
    ensure_eq(jsonv::ast_node_type::object_end, dst.current().type());
}

/// The moved-from half of the contract the header documents, which #223 could only pin one side of. Note that this is
/// `std::invalid_argument` rather than the `std::logic_error` an exhausted reader gives: the two failures are
/// different, and telling them apart is the point.
///
/// \see https://github.com/tgockel/json-voorhees/issues/240
TEST(reader_moved_from_source_throws_invalid_argument)
{
    jsonv::reader src("5");
    ensure(src.next_token());   // document_start -> 5

    jsonv::reader dst(std::move(src));
    ensure(dst.good());

    ensure(!src.good());
    ensure_throws(std::invalid_argument, src.current());
    ensure_throws(std::invalid_argument, src.current_path());
    ensure_throws(std::invalid_argument, src.expect(jsonv::ast_node_type::integer));
    ensure_throws(std::invalid_argument, src.expect({ jsonv::ast_node_type::integer }));
    ensure_throws(std::invalid_argument, src.current_as<jsonv::ast_node::integer>());
}

/// Self-move must leave the reader on the node it was already on. The reference is what keeps the compiler from seeing
/// the self-assignment and warning about it -- the same trick as `object_node_handle_move_assign_to_self`.
///
/// \see https://github.com/tgockel/json-voorhees/issues/240
TEST(reader_move_assign_to_self)
{
    jsonv::reader  reader(R"([ 1, 2, 3 ])");
    jsonv::reader& same = reader;

    ensure(reader.next_token());    // document_start -> [
    ensure(reader.next_token());    // [ -> 1

    reader = std::move(same);

    ensure(reader.good());
    ensure_eq(jsonv::ast_node_type::integer, reader.current().type());
    ensure_eq(jsonv::path::create("[0]"), reader.current_path());
    ensure_eq(std::int64_t(1), reader.current().as<jsonv::ast_node::integer>().value());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Documentation examples                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The two functions below are the worked examples in the `reader` documentation. They live here so the code in those
// comments is known to compile and behave -- the example they replaced had never been compiled and called a macro
// which does not exist. Keep them in step with `include/jsonv/reader.hpp`.

namespace
{

struct my_object
{
    std::int64_t a = 0;
};

/// The example on the \c reader class documentation.
std::optional<my_object> extract_my_object(jsonv::reader& from)
{
    if (!from.expect(jsonv::ast_node_type::object_begin))
        return std::nullopt;

    // Step onto the first key, or onto the } of an empty object.
    if (!from.next_token())
        return std::nullopt;

    my_object out;
    while (from.good() && from.current().type() != jsonv::ast_node_type::object_end)
    {
        // Keys arrive canonical or escaped, depending on whether the source used escape sequences.
        if (!from.expect({ jsonv::ast_node_type::key_canonical, jsonv::ast_node_type::key_escaped }))
            return std::nullopt;

        auto key = from.current().visit_key([](const auto& k) { return std::string(k.value()); });
        if (key == "a")
        {
            if (!from.next_token())
                return std::nullopt;

            if (auto node = from.current_as<jsonv::ast_node::integer>())
                out.a = node->value();
            else
                return std::nullopt;

            // Step off the value and onto the next key, or onto the closing }.
            if (!from.next_token())
                return std::nullopt;
        }
        else
        {
            // A key we do not care about -- skip its value, however large, and land on the next key.
            if (!from.next_key())
                return std::nullopt;
        }
    }
    return out;
}

/// The example on \c reader::next_structure. Find the "a" member of an object and leave the rest of it unread.
std::optional<std::int64_t> find_a(jsonv::reader& from)
{
    if (!from.expect(jsonv::ast_node_type::object_begin))
        return std::nullopt;

    if (!from.next_token())
        return std::nullopt;

    while (from.good() && from.current().type() != jsonv::ast_node_type::object_end)
    {
        if (!from.expect({ jsonv::ast_node_type::key_canonical, jsonv::ast_node_type::key_escaped }))
            return std::nullopt;

        auto key = from.current().visit_key([](const auto& k) { return std::string(k.value()); });
        if (key != "a")
        {
            if (!from.next_key())
                return std::nullopt;

            continue;
        }

        if (!from.next_token())
            return std::nullopt;

        auto node = from.current_as<jsonv::ast_node::integer>();

        // Whatever is left of this object, we are done with it.
        (void) from.next_structure();

        if (node)
            return node->value();
        else
            return std::nullopt;
    }
    return std::nullopt;
}

}

/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(reader_docs_extract_my_object)
{
    {
        jsonv::reader reader(R"({ "a": 7 })");
        ensure(reader.next_token());
        auto res = extract_my_object(reader);
        ensure(res.has_value());
        ensure_eq(std::int64_t(7), res->a);
    }

    {
        // Members which are not "a" are skipped whatever their shape, and order does not matter.
        jsonv::reader reader(R"({ "z": [ 1, { "a": 100 } ], "a": 7, "y": "ignored" })");
        ensure(reader.next_token());
        auto res = extract_my_object(reader);
        ensure(res.has_value());
        ensure_eq(std::int64_t(7), res->a);
    }

    {
        // An empty object leaves the default in place rather than failing.
        jsonv::reader reader("{}");
        ensure(reader.next_token());
        auto res = extract_my_object(reader);
        ensure(res.has_value());
        ensure_eq(std::int64_t(0), res->a);
    }

    {
        // An escaped key still matches. Note this must not be spelled as a raw string containing `\u0061`: the
        // compiler folds a universal-character-name in translation phase 1, so the parser would receive a plain "a"
        // and produce a `key_canonical` node, quietly making this a duplicate of the case above.
        jsonv::reader reader("{ \"\\u0061\": 7 }");
        ensure(reader.next_token());
        auto res = extract_my_object(reader);
        ensure(res.has_value());
        ensure_eq(std::int64_t(7), res->a);
    }

    {
        // An escaped key which is not "a" takes the skip path.
        jsonv::reader reader("{ \"a\\u0062\": 1, \"a\": 7 }");
        ensure(reader.next_token());
        auto res = extract_my_object(reader);
        ensure(res.has_value());
        ensure_eq(std::int64_t(7), res->a);
    }

    {
        jsonv::reader reader("[1]");
        ensure(reader.next_token());
        ensure(!extract_my_object(reader).has_value());
    }
}

/// The example is about `next_structure` leaving the object early, so check that it really does -- the reader must land
/// one past the closing `}` with the unread members skipped.
///
/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(reader_docs_find_a_leaves_the_object)
{
    jsonv::reader reader(R"([ { "a": 7, "b": [ 1, 2, 3 ], "c": 9 }, 42 ])");
    ensure(reader.next_token());    // document_start -> array_begin
    ensure(reader.next_token());    // array_begin     -> object_begin

    auto res = find_a(reader);
    ensure(res.has_value());
    ensure_eq(std::int64_t(7), *res);

    // "b" and "c" were never read: the reader is on the element after the object.
    ensure(reader.good());
    ensure_eq(jsonv::ast_node_type::integer, reader.current().type());
    ensure_eq(std::int64_t(42), reader.current_as<jsonv::ast_node::integer>()->value());
}

/// \see https://github.com/tgockel/json-voorhees/issues/223
TEST(reader_docs_find_a_missing_member)
{
    jsonv::reader reader(R"({ "b": 1, "c": 2 })");
    ensure(reader.next_token());
    ensure(!find_a(reader).has_value());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reader::from_value                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// What a node *means*, as a string which can be compared across sources.
///
/// The token text cannot be compared directly: a value-sourced reader synthesises it, so `"a\nb"` arrives as a
/// canonical token holding a real newline where the text source has a six-byte escaped one. What has to match is
/// everything a caller can actually get out of the node.
static std::string node_payload(const jsonv::ast_node& node)
{
    switch (node.type())
    {
    case jsonv::ast_node_type::object_begin:
        return "n=" + std::to_string(node.as<jsonv::ast_node::object_begin>().element_count());
    case jsonv::ast_node_type::array_begin:
        return "n=" + std::to_string(node.as<jsonv::ast_node::array_begin>().element_count());
    case jsonv::ast_node_type::integer:
        return std::to_string(node.as<jsonv::ast_node::integer>().value());
    case jsonv::ast_node_type::decimal:
        // Compare the bits rather than the value: `-0.0 == 0.0`, and losing the sign of a zero is precisely the
        // round-trip damage this is here to catch.
        return std::to_string(std::bit_cast<std::uint64_t>(node.as<jsonv::ast_node::decimal>().value()));
    case jsonv::ast_node_type::string_canonical:
        return std::string(node.as<jsonv::ast_node::string_canonical>().value());
    case jsonv::ast_node_type::string_escaped:
        return node.as<jsonv::ast_node::string_escaped>().value();
    case jsonv::ast_node_type::key_canonical:
    case jsonv::ast_node_type::key_escaped:
        return node.visit_key([](const auto& key) { return std::string(key.value()); });
    default:
        return std::string();
    }
}

/// Whether \c ensure_value_walk_matches_text compares \c current_path at every node.
///
/// It is \c off only for the multi-megabyte corpus documents. A \c parse_index -sourced reader rebuilds the path by
/// re-walking the tape from the start of the document on every call, so asking for it at every node is quadratic in
/// the document size: eight seconds for the first twenty thousand nodes of `citm_catalog.json`. A value-sourced
/// reader reads its path straight off its frame stack, so it is only the side being compared against which cannot
/// afford this. The structural shapes which make path bookkeeping interesting are covered exhaustively by the
/// example walk tables and by the hand-written documents below.
enum class path_checking
{
    on,
    off,
};

/// Walk \a source both ways -- as a `value` and as the text that value encodes to -- and require the two readers to
/// report the same document, node for node.
///
/// This is the property the whole implementation exists for: `extract<T>(const value&)` reaches an extractor through
/// a reader, and it has to see what it would have seen had the document arrived as text.
static void ensure_value_walk_matches_text(const jsonv::value& source,
                                           path_checking       check_paths = path_checking::on
                                          )
{
    auto text       = to_string(source);
    auto from_text  = jsonv::reader(std::string_view(text));
    auto from_value = jsonv::reader::from_value(source);

    for (std::size_t idx = 0U; ; ++idx)
    {
        JSONV_READER_TESTS_LOG(std::endl << "[" << idx << "] " << from_text.current().type());

        ensure_eq(expected_type(from_text.current().type(), walk_source::value), from_value.current().type());
        ensure_eq(node_payload(from_text.current()),                             node_payload(from_value.current()));

        if (check_paths == path_checking::on)
            ensure_eq(from_text.current_path(), from_value.current_path());

        bool text_more  = from_text.next_token();
        bool value_more = from_value.next_token();
        ensure_eq(text_more, value_more);

        if (!text_more)
            break;
    }
}

/// Named apart from its `value` sibling rather than overloaded on purpose -- `value` converts implicitly from
/// `std::string` and from `const char*`, so a pair of overloads taking `const value&` and `std::string_view` is
/// ambiguous for every caller. That is the same collision which makes `reader::from_value` a named factory.
static void ensure_parsed_walk_matches_text(std::string_view source)
{
    ensure_value_walk_matches_text(jsonv::parse(source));
}

/// One test per corpus document, so a failure names the file it came from.
class reader_from_value_matches_text_on_corpus final :
        public jsonv_test::unit_test
{
public:
    reader_from_value_matches_text_on_corpus(std::string filename, path_checking check_paths) :
            jsonv_test::unit_test("reader_from_value_matches_text_on_corpus/" + filename),
            _filename(std::move(filename)),
            _check_paths(check_paths)
    { }

    static std::vector<std::unique_ptr<jsonv_test::unit_test>> make_tests()
    {
        std::vector<std::unique_ptr<jsonv_test::unit_test>> out;

        // Deliberately not the whole tree: `data/json_checker` is full of documents which are supposed to fail to
        // parse, and there is nothing to compare for those.
        for (const char* name : { "blns.json", "paths.json" })
            out.emplace_back(std::make_unique<reader_from_value_matches_text_on_corpus>(name, path_checking::on));

        for (const char* name : { "canada.json", "citm_catalog.json", "generated.json" })
            out.emplace_back(std::make_unique<reader_from_value_matches_text_on_corpus>(name, path_checking::off));

        return out;
    }

protected:
    virtual void run_impl() override
    {
        std::ifstream      in(jsonv_test::test_path(_filename));
        std::ostringstream buffer;
        buffer << in.rdbuf();
        auto src = std::move(buffer).str();
        ensure(!src.empty());

        ensure_value_walk_matches_text(jsonv::parse(src), _check_paths);
    }

private:
    std::string   _filename;
    path_checking _check_paths;
};

static std::vector<std::unique_ptr<jsonv_test::unit_test>> corpus_tests =
        reader_from_value_matches_text_on_corpus::make_tests();

/// The fuzz seeds, which is where the awkward documents live: escapes of every shape, surrogate pairs, embedded
/// NULs, duplicate keys, numbers at the edge of their types, and nesting at the depth limit.
///
/// Roughly half of them are supposed to fail to parse -- that is what they are seeds *for* -- so those are skipped
/// rather than being a failure here. The count is asserted so that a corpus which stopped parsing entirely, or a
/// directory which moved, cannot leave this passing vacuously.
TEST(reader_from_value_matches_text_on_fuzz_corpus)
{
    // `test_path` is rooted at `src/jsonv-tests/data`, and the fuzz seeds are a sibling of that tree.
    auto root = jsonv_test::test_path("../../jsonv-fuzz/corpus");

    std::size_t compared = 0U;
    jsonv_test::recursive_directory_for_each(root, ".json", [&](const std::string& path)
        {
            std::ifstream      in(path);
            std::ostringstream buffer;
            buffer << in.rdbuf();
            auto src = std::move(buffer).str();
            ensure(!src.empty());

            jsonv::value parsed;
            try
            {
                parsed = jsonv::parse(src);
            }
            catch (const std::exception&)
            {
                // Not just `parse_error`: a number which no `double` can hold, like the `1e309` in
                // `numbers_extremes.json`, fails later than that and arrives as `std::invalid_argument`. Either way
                // there is no `value` to compare, and the `try` covers nothing but the parse.
                return;
            }

            // Five of the seeds hold byte sequences which are not valid UTF-8 at all. `parse` accepts those into a
            // `value`, but encoding one does not survive a re-parse: `to_string` turns the bytes into `\u0000`, or
            // into an unpaired surrogate which fails to parse at all. That is a defect on the text side -- neither
            // reader is involved in it -- and it leaves the text path useless as a baseline for those documents.
            // The check reaches for neither reader, so it cannot hide a mistake in one.
            try
            {
                if (jsonv::parse(to_string(parsed)) != parsed)
                    return;
            }
            catch (const std::exception&)
            {
                return;
            }

            JSONV_READER_TESTS_LOG(std::endl << jsonv_test::filename(path));
            ensure_value_walk_matches_text(parsed);
            ++compared;
        });

    ensure_ge(compared, std::size_t(8U));
}

TEST(reader_from_value_matches_text)
{
    for (const auto& example : examples)
        ensure_parsed_walk_matches_text(example.source);

    ensure_parsed_walk_matches_text("{}");
    ensure_parsed_walk_matches_text("[]");
    ensure_parsed_walk_matches_text("null");
    ensure_parsed_walk_matches_text("4");
    ensure_parsed_walk_matches_text(R"("bare string")");
    ensure_parsed_walk_matches_text(R"({ "a": {}, "b": [], "c": { "d": [ {}, [] ] } })");
    ensure_parsed_walk_matches_text(R"([ true, false, null, 0, -0.5, "", "é" ])");
    // The same string spelled as an escape. It is `string_escaped` down the text path and `string_canonical`
    // down the value path, which is the one difference the two walks are allowed to have.
    ensure_parsed_walk_matches_text(R"([ "\u00e9", "a\nb", "\"", "\\" ])");
}

/// Numbers a careless round trip damages. Each is built as a `value` rather than parsed, so the only thing under test
/// is what `impl_value` synthesises.
///
/// \see https://github.com/tgockel/json-voorhees/issues/208
TEST(reader_from_value_number_edges)
{
    for (std::int64_t x : { std::numeric_limits<std::int64_t>::min(),
                            std::numeric_limits<std::int64_t>::max(),
                            std::int64_t(0),
                            std::int64_t(-1),
                            (std::int64_t(1) << 53) + 1,
                            -((std::int64_t(1) << 53) + 1),
                          }
        )
    {
        auto reader = jsonv::reader::from_value(jsonv::value(x));
        ensure(reader.next_token());
        auto node = reader.current_as<jsonv::ast_node::integer>();
        ensure(node.has_value());
        ensure_eq(x, node->value());
    }

    for (double x : { 0.0,
                      -0.0,
                      2.0,
                      3.14,
                      0.1,
                      0.1 + 0.2,
                      1.7976931348623157e308,
                      5e-324,
                      -1.2345678901234567e-300,
                    }
        )
    {
        auto reader = jsonv::reader::from_value(jsonv::value(x));
        ensure(reader.next_token());

        // An integral decimal has to stay a decimal: `2.0` printed as `2` would arrive as `kind::integer`.
        auto node = reader.current_as<jsonv::ast_node::decimal>();
        ensure(node.has_value());
        ensure_eq(std::bit_cast<std::uint64_t>(x), std::bit_cast<std::uint64_t>(node->value()));
    }
}

/// A non-finite `double` has no JSON representation, and the encoder writes `null` for one. A value-sourced reader
/// has to agree, or a NaN would mean one thing in an encoded document and another through an extractor.
TEST(reader_from_value_non_finite_decimal_is_null)
{
    for (double x : { std::numeric_limits<double>::quiet_NaN(),
                      std::numeric_limits<double>::infinity(),
                      -std::numeric_limits<double>::infinity(),
                    }
        )
    {
        auto value = jsonv::value(x);
        ensure_eq(jsonv::kind::decimal, value.kind());

        auto reader = jsonv::reader::from_value(value);
        ensure(reader.next_token());
        ensure_eq(jsonv::ast_node_type::literal_null, reader.current().type());

        // ...which is what the text path does too.
        ensure_value_walk_matches_text(value);
    }
}

/// A `value` holds decoded bytes, so every string and key is canonical no matter what it contains -- including the
/// bytes JSON would have to escape. Nothing re-lexes a synthesised token, and the canonical `string_from_token`
/// strips exactly the outer two bytes, so a `"` inside one is harmless.
TEST(reader_from_value_strings_are_canonical)
{
    const std::string contents[] =
    {
        "",
        "plain",
        "has \" quote",
        "has \\ backslash",
        std::string("has \0 nul", 9U),
        "\n\t\r",
        "\xc3\xa9 \xe2\x98\x83 \xf0\x9f\x92\xa9",     // é ☃ 💩
        "\x7f",
    };

    for (const auto& content : contents)
    {
        jsonv::value obj = jsonv::object({ { content, content } });

        auto reader = jsonv::reader::from_value(obj);
        ensure(reader.next_token());    // document_start -> object_begin
        ensure(reader.next_token());    // object_begin   -> the key

        auto key = reader.current_as<jsonv::ast_node::key_canonical>();
        ensure(key.has_value());
        ensure_eq(content, std::string(key->value()));

        ensure(reader.next_token());    // the key -> the string

        auto str = reader.current_as<jsonv::ast_node::string_canonical>();
        ensure(str.has_value());
        ensure_eq(content, std::string(str->value()));

        // The raw token is the bytes with a quotation mark on each end, which for these is not valid JSON on its own.
        ensure_eq(content.size() + 2U, str->token_raw().size());
        ensure_eq('"', str->token_raw().front());
        ensure_eq('"', str->token_raw().back());
    }
}

/// The frame stack is a `std::vector` and the walk is a loop, so depth costs heap instead of call frames.
TEST(reader_from_value_deep_nesting)
{
    constexpr std::size_t depth = 4096U;

    // Built by moving rather than with `jsonv::array({ ... })`, whose `initializer_list` elements are `const` -- that
    // spelling deep-copies the whole nesting once per level.
    jsonv::value nested = jsonv::array();
    for (std::size_t n = 0U; n < depth; ++n)
    {
        jsonv::value outer = jsonv::array();
        outer.push_back(std::move(nested));
        nested = std::move(outer);
    }

    auto reader = jsonv::reader::from_value(std::move(nested));

    std::size_t opens = 0U;
    std::size_t peak  = 0U;
    while (reader.next_token())
    {
        if (reader.current().type() == jsonv::ast_node_type::array_begin)
        {
            ++opens;
            peak = std::max(peak, reader.current_path().size());
        }
    }

    ensure_eq(depth + 1U, opens);
    // The innermost `[` is inside `depth` enclosing arrays, each contributing one index to the path.
    ensure_eq(depth, peak);
}

/// Where `next_value` lands, as an index into the token stream.
///
/// The position is recovered by counting what is *left* rather than by hunting for a matching node. The tape-backed
/// twin of this test identifies the landing spot with `token_raw().data()`, which works only because both of its
/// readers point into one source text; two value-sourced readers have independent arenas, and a node type with a
/// path does not pin a position uniquely.
static std::size_t next_value_destination(jsonv::reader& reader, std::size_t total)
{
    if (!reader.next_value())
        return std::size_t(-1);

    std::size_t remaining = 0U;
    while (reader.next_token())
        ++remaining;

    return total - remaining;
}

/// Count the way out by hand. `next_value` steps *over* the value the reader is on, so for a structure it lands one
/// past the matching close token -- landing on the close would leave an object loop looking at what it cannot tell
/// apart from the end of the enclosing object -- and for anything else it is a single step.
static std::size_t next_value_expected(std::string_view text, std::size_t idx, std::size_t total)
{
    auto reader = jsonv::reader(text);
    for (std::size_t n = 0U; n < idx; ++n)
        ensure(reader.next_token());

    if (auto type = reader.current().type();
        type != jsonv::ast_node_type::object_begin && type != jsonv::ast_node_type::array_begin
       )
    {
        return idx < total ? idx + 1U : std::size_t(-1);
    }

    std::size_t depth = 0U;
    for (std::size_t n = idx; ; ++n)
    {
        auto tok = reader.current().type();
        if (tok == jsonv::ast_node_type::object_begin || tok == jsonv::ast_node_type::array_begin)
        {
            ++depth;
        }
        else if (tok == jsonv::ast_node_type::object_end || tok == jsonv::ast_node_type::array_end)
        {
            --depth;
            if (depth == 0U)
                return n + 1U;
        }

        ensure(reader.next_token());
    }
}

/// `next_value` has to land in the same place whichever source the reader has. This is checked against a hand-counted
/// walk *and* across the two sources, at every position: an oracle written only against the reader under test would
/// happily confirm whatever that reader does.
///
/// The failure this guards against is quiet. An extractor which skips an uninteresting member whose value is an
/// object would, on a reader left sitting on that object's `}`, read it as the end of the enclosing object and drop
/// every member after it -- against one source only.
static void ensure_next_value_agrees(const jsonv::value& source)
{
    auto text = to_string(source);

    std::size_t total = 0U;
    {
        auto reader = jsonv::reader(std::string_view(text));
        while (reader.next_token())
            ++total;
    }

    for (std::size_t idx = 0U; idx <= total; ++idx)
    {
        JSONV_READER_TESTS_LOG(std::endl << "next_value from [" << idx << "]");

        auto expected = next_value_expected(text, idx, total);

        auto from_text = jsonv::reader(std::string_view(text));
        auto from_value = jsonv::reader::from_value(source);
        for (std::size_t n = 0U; n < idx; ++n)
        {
            ensure(from_text.next_token());
            ensure(from_value.next_token());
        }

        ensure_eq(expected, next_value_destination(from_text,  total));
        ensure_eq(expected, next_value_destination(from_value, total));
    }
}

TEST(reader_from_value_next_value_agrees_with_counting_the_way_out)
{
    for (const auto& example : examples)
        ensure_next_value_agrees(jsonv::parse(example.source));

    // The shapes where landing on the close rather than past it diverges: a member whose value is a structure, a
    // structure at the root, and a structure as an array element.
    ensure_next_value_agrees(jsonv::parse(R"({ "a": {}, "b": 123 })"));
    ensure_next_value_agrees(jsonv::parse(R"({ "a": { "x": 1 }, "b": 2 })"));
    ensure_next_value_agrees(jsonv::parse(R"({ "a": [ 1, 2 ], "b": 2 })"));
    ensure_next_value_agrees(jsonv::parse(R"([ [ 1, 2 ], 3 ])"));
    ensure_next_value_agrees(jsonv::parse(R"([ [ [ [ [ 1 ] ] ] ], [], [ {} ] ])"));
    ensure_next_value_agrees(jsonv::parse(R"({ "a": {}, "b": [ { "c": 1 } ] })"));
    ensure_next_value_agrees(jsonv::parse("4"));
    ensure_next_value_agrees(jsonv::parse("{}"));
    ensure_next_value_agrees(jsonv::parse("[]"));
}

/// The member after one whose value was skipped has to still be there. This is the concrete shape of the bug the
/// position-by-position check above generalizes, spelled out so a regression reads as what it costs a caller.
static void ensure_skipping_a_member_keeps_the_next_one(jsonv::reader& reader)
{
    ensure(reader.next_token());    // document_start -> object_begin
    ensure(reader.next_token());    // object_begin   -> "a"
    ensure(reader.next_token());    // "a"            -> its value

    ensure(reader.next_value());

    auto key = reader.current_as<jsonv::ast_node::key_canonical>();
    ensure(key.has_value());
    ensure_eq(std::string("b"), std::string(key->value()));
}

TEST(reader_next_value_over_a_member_lands_on_the_next_key)
{
    for (const char* src : { R"({ "a": {}, "b": 123 })",
                             R"({ "a": { "x": 1 }, "b": 123 })",
                             R"({ "a": [ 1, 2 ], "b": 123 })",
                             R"({ "a": 1, "b": 123 })",
                           }
        )
    {
        auto from_text = jsonv::reader(src);
        ensure_skipping_a_member_keeps_the_next_one(from_text);

        auto from_value = jsonv::reader::from_value(jsonv::parse(src));
        ensure_skipping_a_member_keeps_the_next_one(from_value);
    }
}

/// The contract the arena exists for: a view taken from `current` stays readable after the reader has moved on. A
/// `parse_index` source provides that for free by pointing into the source text, and two reader sources disagreeing
/// about it would be a miserable bug to find in an extractor.
TEST(reader_from_value_views_survive_next_token)
{
    auto reader = jsonv::reader::from_value(jsonv::parse(R"({ "key": "value", "n": 12345 })"));

    ensure(reader.next_token());    // document_start -> object_begin
    ensure(reader.next_token());    // object_begin   -> "key"

    auto key_token = reader.current().token_raw();
    auto key_text  = reader.current_as<jsonv::ast_node::key_canonical>()->value();

    ensure(reader.next_token());    // "key" -> "value"

    auto string_token = reader.current().token_raw();
    auto string_text  = reader.current_as<jsonv::ast_node::string_canonical>()->value();

    // Walk the rest of the document, which is what would recycle a single scratch buffer.
    while (reader.next_token())
        (void) reader.current().token_raw();

    ensure_eq(std::string(R"("key")"),   std::string(key_token));
    ensure_eq(std::string("key"),        std::string(key_text));
    ensure_eq(std::string(R"("value")"), std::string(string_token));
    ensure_eq(std::string("value"),      std::string(string_text));
}

/// The rvalue overload keeps its source alive. Under AddressSanitizer this is where a frame pointing into a
/// destroyed `value` shows up.
TEST(reader_from_value_rvalue_owns_its_source)
{
    auto reader = []
        {
            jsonv::value source = jsonv::parse(R"({ "a": [ 1, "two", 3.5 ], "b": { "c": null } })");
            return jsonv::reader::from_value(std::move(source));
        }();

    std::size_t count = 0U;
    do
    {
        (void) reader.current().token_raw();
        (void) reader.current_path();
        ++count;
    } while (reader.next_token());

    ensure_eq(std::size_t(15U), count);
}

/// Every `next_` reports an exhausted reader by returning `false`, and the accessors throw after that. A
/// value-sourced reader is no different from a tape-sourced one here.
///
/// \see https://github.com/tgockel/json-voorhees/issues/213
TEST(reader_from_value_exhausted)
{
    auto reader = jsonv::reader::from_value(jsonv::parse("[ 1 ]"));
    while (reader.next_token())
        ;

    ensure(!reader.good());
    ensure(!reader.next_token());
    ensure(!reader.next_value());
    ensure(!reader.next_structure());
    ensure(!reader.next_key());
    ensure_throws(std::logic_error, reader.current());
    ensure_throws(std::logic_error, reader.current_path());
}

TEST(reader_from_value_moved_from)
{
    auto reader = jsonv::reader::from_value(jsonv::parse(R"({ "a": 1 })"));
    auto moved  = std::move(reader);

    ensure(!reader.good());
    ensure(!reader.next_token());
    ensure_throws(std::invalid_argument, reader.current());
    ensure_throws(std::invalid_argument, reader.current_path());

    ensure(moved.good());
    ensure_eq(jsonv::ast_node_type::document_start, moved.current().type());
}

/// `next_key` is only valid on a key or on the opening `{`, whichever source the reader has.
///
/// \see https://github.com/tgockel/json-voorhees/issues/213
TEST(reader_from_value_next_key_on_non_key_node_throws)
{
    auto reader = jsonv::reader::from_value(jsonv::parse("[1, 2]"));
    ensure(reader.next_token());    // document_start -> array_begin
    ensure_throws(std::invalid_argument, reader.next_key());
}

/// The reference overload does not copy, so the caller keeps ownership -- and reading the same value twice has to
/// give the same answer both times.
TEST(reader_from_value_reference_does_not_consume)
{
    jsonv::value source = jsonv::parse(R"({ "a": [ 1, 2 ], "b": "x" })");

    auto first  = to_string(source);
    ensure_value_walk_matches_text(source);
    ensure_value_walk_matches_text(source);
    ensure_eq(first, to_string(source));
}


TEST(reader_current_path_on_an_unmatched_close)
{
    // `parse_index::parse` does not validate, so a reader can sit on a close token with nothing open. Asking where it
    // is must answer rather than run off the end of the element stack.
    for (const auto* source : { "}", "]", "{}}", "[1]]" })
    {
        jsonv::reader rdr(source);
        while (rdr.good())
        {
            (void) rdr.current_path();
            (void) rdr.next_token();
        }
    }
}

TEST(reader_validate_passes_text_which_parsed)
{
    // The answer is about the source, so it holds wherever the cursor is.
    jsonv::reader rdr(R"({ "a": [ 1, 2 ] })");
    rdr.validate();

    (void) rdr.next_token();
    (void) rdr.next_token();
    rdr.validate();
}

TEST(reader_validate_throws_for_text_which_did_not_parse)
{
    // Constructing the reader succeeds -- parsing never throws -- so this is the only place to find out.
    jsonv::reader rdr("[ 1, 2");
    ensure(rdr.good());

    try
    {
        rdr.validate();
        ensure(!"parse_error was not thrown");
    }
    catch (const jsonv::parse_error& ex)
    {
        ensure(ex.character().has_value());
    }
}

TEST(reader_validate_never_throws_for_a_value)
{
    jsonv::reader::from_value(jsonv::parse("[ 1, 2 ]")).validate();
}

TEST(reader_validate_on_a_moved_from_reader_throws)
{
    jsonv::reader rdr("5");
    jsonv::reader other(std::move(rdr));

    ensure_throws(std::invalid_argument, rdr.validate());
    other.validate();
}

TEST(reader_owns_source)
{
    std::string  text   = "5";
    jsonv::value in_mem = 5;

    // The caller keeps these alive.
    ensure(!jsonv::reader(std::string_view(text)).owns_source());
    ensure(!jsonv::reader("5").owns_source());
    ensure(!jsonv::reader(jsonv::parse_index::parse(text)).owns_source());
    ensure(!jsonv::reader::from_value(in_mem).owns_source());

    // The reader keeps these alive.
    ensure(jsonv::reader(std::string(text)).owns_source());
    ensure(jsonv::reader::from_value(jsonv::value(5)).owns_source());

    // Ownership goes with the implementation when the reader moves.
    jsonv::reader owning{ std::string(text) };
    jsonv::reader moved(std::move(owning));
    ensure(!owning.owns_source());
    ensure(moved.owns_source());
}

}
