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

#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>

#include <iostream>
#include <random>
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
              jsonv::string_view   path,
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
    jsonv::string_view     name;
    jsonv::string_view     source;
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

static void walk_expecting_next_struct_at_index(jsonv::string_view            src,
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

static void walk_expecting_next_key_at_index(jsonv::string_view            src,
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
static void walk_random_expecting(jsonv::string_view            source,
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

}
