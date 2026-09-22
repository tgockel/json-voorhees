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

#include <jsonv/ast.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>

#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>
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

}

template <typename FShouldJumpNextKey, typename FShouldJumpNextStruct>
static void walk_expecting(jsonv::reader&                reader,
                           const std::vector<walk_info>& expected,
                           FShouldJumpNextKey&&          should_jump_next_key,
                           FShouldJumpNextStruct&&       should_jump_next_struct
                          )
{

    ensure_eq(jsonv::ast_node_type::document_start, reader.current().type());
    ensure(reader.next_token());

    for (std::size_t idx = 0U; idx < expected.size(); /* inline */)
    {
        auto current = reader.current();

        ensure_eq(expected[idx].type, current.type());
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
    reader_example_walkthrough_no_jump_test(example_data_type data) :
            jsonv_test::unit_test(std::string("reader_example_walkthrough_no_jump/") + std::string(data.name)),
            _data(std::move(data))
    { }

    static std::vector<std::unique_ptr<jsonv_test::unit_test>> make_tests(const example_data_type& data)
    {
        std::vector<std::unique_ptr<jsonv_test::unit_test>> out;
        out.emplace_back(std::make_unique<reader_example_walkthrough_no_jump_test>(data));
        return out;
    }

protected:
    virtual void run_impl() override
    {
        jsonv::reader reader(_data.source);
        walk_expecting(reader, _data.expected, [](auto&&...) { return false; }, [](auto&&...) { return false; });
    }

private:
    example_data_type _data;
};

static void walk_expecting_next_struct_at_index(std::string_view            src,
                                                const std::vector<walk_info>& expected,
                                                std::size_t                   jump_from_idx
                                               )
{
    JSONV_READER_TESTS_LOG(std::endl << "to " << jump_from_idx << "...");
    jsonv::reader reader(src);
    walk_expecting(reader,
                   expected,
                   [](auto&&...) { return false; },
                   [&](auto&&, auto idx) { return idx == jump_from_idx; }
                  );
}

class reader_example_walkthrough_next_struct_at_index final :
        public jsonv_test::unit_test
{
public:
    reader_example_walkthrough_next_struct_at_index(example_data_type data, std::size_t next_struct_at_idx) :
            jsonv_test::unit_test(std::string("reader_example_walkthrough_next_struct_at_index/")
                                  + std::string(data.name) + "/" + std::to_string(next_struct_at_idx)),
            _data(std::move(data)),
            _next_struct_at_idx(next_struct_at_idx)
    { }

    static std::vector<std::unique_ptr<jsonv_test::unit_test>> make_tests(const example_data_type& data)
    {
        std::vector<std::unique_ptr<jsonv_test::unit_test>> out;
        out.reserve(data.expected.size());
        for (std::size_t idx = 0U; idx < data.expected.size(); ++idx)
            out.emplace_back(std::make_unique<reader_example_walkthrough_next_struct_at_index>(data, idx));
        return out;
    }

protected:
    virtual void run_impl() override
    {
        walk_expecting_next_struct_at_index(_data.source, _data.expected, _next_struct_at_idx);
    }

private:
    example_data_type _data;
    std::size_t       _next_struct_at_idx;
};

static void walk_expecting_next_key_at_index(std::string_view            src,
                                             const std::vector<walk_info>& expected,
                                             std::size_t                   jump_from_idx
                                            )
{
    JSONV_READER_TESTS_LOG(std::endl << "to " << jump_from_idx << "...");
    jsonv::reader reader(src);
    walk_expecting(reader,
                   expected,
                   [&](auto&&, auto idx) { return idx == jump_from_idx; },
                   [](auto&&...) { return false; }
                  );
}

class reader_example_walkthrough_next_key_at_index final :
        public jsonv_test::unit_test
{
public:
    reader_example_walkthrough_next_key_at_index(example_data_type data, std::size_t next_key_at_idx) :
            jsonv_test::unit_test(std::string("reader_example_walkthrough_next_key_at_index/")
                                  + std::string(data.name) + "/" + std::to_string(next_key_at_idx)),
            _data(std::move(data)),
            _next_key_at_idx(next_key_at_idx)
    { }

    static std::vector<std::unique_ptr<jsonv_test::unit_test>> make_tests(const example_data_type& data)
    {
        std::vector<std::unique_ptr<jsonv_test::unit_test>> out;
        out.reserve(data.expected.size());
        for (std::size_t idx = 0U; idx < data.expected.size(); ++idx)
        {
            if (data.expected[idx].next_key_idx != 0U)
                out.emplace_back(std::make_unique<reader_example_walkthrough_next_key_at_index>(data, idx));
        }
        return out;
    }

protected:
    virtual void run_impl() override
    {
        walk_expecting_next_key_at_index(_data.source, _data.expected, _next_key_at_idx);
    }

private:
    example_data_type _data;
    std::size_t       _next_key_at_idx;
};

/// Walk through \a source, randomly calling \c next_key with \a next_key_probability (when current position is a key
/// type) and \c next_struct with \a next_struct_probability. The random number generator is seeded with \a rng_seed so
/// behavior is deterministic.
static void walk_random_expecting(std::string_view            source,
                                  const std::vector<walk_info>& expected,
                                  double                        next_key_probability    = 0.0,
                                  double                        next_struct_probability = 0.0,
                                  std::size_t                   rng_seed                = 0U
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

    jsonv::reader reader(source);
    return walk_expecting(reader,
                          expected,
                          [&](auto&&...) { return next_key_dist(rng); },
                          [&](auto&&...) { return next_struct_dist(rng); }
                         );
}

class reader_example_walkthrough_randomly final :
        public jsonv_test::unit_test
{
public:
    reader_example_walkthrough_randomly(example_data_type data,
                                        double            next_struct_probability,
                                        double            next_key_probability,
                                        std::size_t       seed
                                       ) :
            jsonv_test::unit_test(std::string("reader_example_walkthrough_randomly/")
                                  + std::string(data.name)
                                  + "/P(next_struct=" + std::to_string(next_struct_probability)
                                       + ",next_key=" + std::to_string(next_key_probability) + ")/"
                                  + std::to_string(seed)),
            _data(std::move(data)),
            _next_struct_probability(next_struct_probability),
            _next_key_probability(next_key_probability),
            _seed(seed)
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
                    out.emplace_back(std::make_unique<reader_example_walkthrough_randomly>(data,
                                                                                           next_struct_p,
                                                                                           next_key_p,
                                                                                           dist(rng)
                                                                                          )
                                    );
                }
            }
        }

        // Throw in some random distributions for good measure
        std::uniform_real_distribution next_struct_dist;
        std::uniform_real_distribution next_key_dist;
        for (std::size_t count = 0U; count < data.expected.size(); ++count)
        {
            out.emplace_back(std::make_unique<reader_example_walkthrough_randomly>(data,
                                                                                   next_struct_dist(rng),
                                                                                   next_key_dist(rng),
                                                                                   dist(rng)
                                                                                  )
                            );
        }

        return out;
    }

protected:
    virtual void run_impl() override
    {
        walk_random_expecting(_data.source, _data.expected, _next_key_probability, _next_struct_probability, _seed);
    }

private:
    example_data_type _data;
    double            _next_struct_probability;
    double            _next_key_probability;
    std::size_t       _seed;
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
/// `reader::impl::current` as a `std::logic_error`. `std::invalid_argument` is only the moved-from case, which cannot
/// be reached from a test: `reader` does not currently compile when moved outside of `reader.cpp`.
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

}
