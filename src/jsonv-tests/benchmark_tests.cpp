/** \file
 *
 *  Copyright (c) 2016-2019 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"
#include "chrono_io.hpp"
#include "filesystem_util.hpp"
#include "stopwatch.hpp"

#include <jsonv/algorithm.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/parse_index.hpp>
#include <jsonv/serialization_builder.hpp>
#include <jsonv/value.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace jsonv_test
{

using namespace jsonv;

static const unsigned iterations = JSONV_DEBUG ? 1 : 100;

template <typename THolster, typename FLoader>
static void run_test(FLoader load, const std::string& from)
{
    stopwatch timer;
    for (unsigned cnt = 0; cnt < iterations; ++cnt)
    {
        THolster src_data{load(from)};
        {
            JSONV_TEST_TIME(timer);
            (void) parse(src_data);
        }
    }
    std::cout << timer.get();
}

// Times stage-1 only: parse_index::parse on a string source. Skips the extract phase
// (jsonv::value materialization), so the number reflects the parse_index hot path alone.
static void run_parse_index_test(const std::string& path)
{
    std::ifstream inputfile(path.c_str());
    std::string src;
    inputfile.seekg(0, std::ios::end);
    src.reserve(inputfile.tellg());
    inputfile.seekg(0, std::ios::beg);
    src.assign(std::istreambuf_iterator<char>(inputfile), std::istreambuf_iterator<char>());

    stopwatch timer;
    for (unsigned cnt = 0; cnt < iterations; ++cnt)
    {
        JSONV_TEST_TIME(timer);
        auto idx = parse_index::parse(src);
        (void)idx;
    }
    std::cout << timer.get();
}

template <typename THolster>
class benchmark_test :
        public unit_test
{
public:
    using loader = std::string (*)(const std::string&);

public:
    benchmark_test(loader load, std::string source_desc, std::string path) :
            unit_test(std::string("benchmark/") + source_desc + "/" + filename(path)),
            load(load),
            path(std::move(path))
    { }

    virtual void run_impl() override
    {
        run_test<THolster>(load, path);
    }

private:
    loader      load;
    std::string path;
};

class parse_index_benchmark_test :
        public unit_test
{
public:
    explicit parse_index_benchmark_test(std::string path) :
            unit_test(std::string("benchmark/parse_index/") + filename(path)),
            path(std::move(path))
    { }

    virtual void run_impl() override
    {
        run_parse_index_test(path);
    }

private:
    std::string path;
};

class benchmark_test_initializer
{
public:
    explicit benchmark_test_initializer(const std::string& rootpath)
    {
        recursive_directory_for_each(rootpath, ".json", [&, this] (const std::string& path)
        {
            if (path.find("fail") == std::string::npos)
            {
                _tests.emplace_back(new benchmark_test<std::ifstream>([] (const std::string& p) { return p; },
                                                                      "ifstream",
                                                                      path
                                                                     )
                                   );
                _tests.emplace_back(new benchmark_test<std::string>(load_from_file, "string", path));
                _tests.emplace_back(new parse_index_benchmark_test(path));
            }
        });
    }

    static std::string load_from_file(const std::string& path)
    {
        std::ifstream inputfile(path.c_str());
        std::string out;
        inputfile.seekg(0, std::ios::end);
        out.reserve(inputfile.tellg());
        inputfile.seekg(0, std::ios::beg);

        out.assign(std::istreambuf_iterator<char>(inputfile), std::istreambuf_iterator<char>());
        return out;
    }

private:
    std::deque<std::unique_ptr<unit_test>> _tests;
} benchmark_test_initializer_instance(test_path(""));

// Times the two ways of putting a value into an object against each other. Issue #152 was that `insert` deep-copied
// the pair it was handed, which made it arbitrarily slower than `operator[]` as the inserted value grew; the two
// should now cost about the same.
static void run_object_insert_test(bool use_insert)
{
    constexpr std::size_t entries = 200;

    // Nested, so that a deep copy of it is much more expensive than moving the handle.
    value payload = object();
    for (int idx = 0; idx < 64; ++idx)
        payload[std::to_string(idx)] = array({ idx, "string payload", double(idx) * 1.5 });

    std::vector<std::string> keys;
    for (std::size_t idx = 0; idx < entries; ++idx)
        keys.emplace_back("key-" + std::to_string(idx));

    stopwatch timer;
    for (unsigned cnt = 0; cnt < iterations; ++cnt)
    {
        std::vector<value> sources(entries, payload);
        value              dst = object();
        {
            JSONV_TEST_TIME(timer);
            for (std::size_t idx = 0; idx < entries; ++idx)
            {
                if (use_insert)
                    dst.insert({ keys[idx], std::move(sources[idx]) });
                else
                    dst[keys[idx]] = std::move(sources[idx]);
            }
        }
    }
    std::cout << timer.get();
}

class object_insert_benchmark_test :
        public unit_test
{
public:
    object_insert_benchmark_test(const std::string& name, bool use_insert) :
            unit_test("benchmark/object_insert/" + name),
            use_insert(use_insert)
    { }

    virtual void run_impl() override
    {
        run_object_insert_test(use_insert);
    }

private:
    bool use_insert;
};

object_insert_benchmark_test object_insert_benchmark_insert_instance("insert", true);
object_insert_benchmark_test object_insert_benchmark_subscript_instance("subscript", false);

// The `extract/` rows time extraction to a C++ type. Each case runs three ways over the same input and target type:
// `parse_then_extract` builds a `value` and extracts from that, which is what extracting from text cost before #150;
// `from_text` extracts straight off the parse index; and `from_value` extracts from a `value` parsed before the clock
// starts, which is the case reading through `reader::from_value` must not make any slower.
//
// Fewer iterations than the rows above: every input here is over a megabyte, and at 100 iterations these rows would
// take three times as long as the rest of the suite put together.
static const unsigned extract_iterations = JSONV_DEBUG ? 1 : 10;

namespace
{

struct citm_price
{
    std::int64_t amount;
    std::int64_t audience_sub_category_id;
    std::int64_t seat_category_id;
};

struct citm_area
{
    std::int64_t              area_id;
    std::vector<std::int64_t> block_ids;
};

struct citm_seat_category
{
    std::vector<citm_area> areas;
    std::int64_t           seat_category_id;
};

struct citm_performance
{
    std::int64_t                    event_id;
    std::int64_t                    id;
    std::optional<std::string>      logo;
    std::optional<std::string>      name;
    std::vector<citm_price>         prices;
    std::vector<citm_seat_category> seat_categories;
    std::optional<std::string>      seat_map_image;
    std::int64_t                    start;
    std::string                     venue_code;
};

/// Every member of every performance, which is most of `citm_catalog.json`. The document's other top-level keys are
/// objects keyed by ID, which nothing in the library extracts to.
struct citm_catalog
{
    std::vector<citm_performance> performances;
};

/// Two of each performance's nine members, and one of the document's eleven top-level keys. Nearly everything is
/// skipped, which a reader over the parse index does in constant time per subtree and the old pipeline built anyway.
struct citm_performance_ids
{
    std::int64_t event_id;
    std::int64_t id;
};

struct citm_catalog_ids
{
    std::vector<citm_performance_ids> performances;
};

/// Containers of scalars all the way down -- 480 rings of 55563 `[lon, lat]` points -- each of which can be told its
/// size by the array's opening token rather than growing to it.
struct canada_geometry
{
    std::vector<std::vector<std::vector<double>>> coordinates;
};

struct canada_feature
{
    canada_geometry geometry;
};

struct canada_collection
{
    std::vector<canada_feature> features;
};

/// The state of a support ticket, which is what an enum-heavy document is full of. Half the spellings are past every
/// small-string buffer, so a `value` built to hold one costs an allocation for its text as well as one for itself.
enum class ticket_state
{
    open,
    closed,
    pending,
    duplicate,
    awaiting_customer_response,
    escalated_to_engineering,
    resolved_without_any_change,
    waiting_on_third_party_vendor,
};

/// Built on first use from a `run_impl` rather than during static initialization, so a mistake in it fails the row
/// which asked instead of the whole binary.
const formats& extract_benchmark_formats()
{
    static const formats instance =
        formats_builder()
            .type<citm_price>()
                .member("amount",                &citm_price::amount)
                .member("audienceSubCategoryId", &citm_price::audience_sub_category_id)
                .member("seatCategoryId",        &citm_price::seat_category_id)
            .type<citm_area>()
                .member("areaId",   &citm_area::area_id)
                .member("blockIds", &citm_area::block_ids)
            .type<citm_seat_category>()
                .member("areas",          &citm_seat_category::areas)
                .member("seatCategoryId", &citm_seat_category::seat_category_id)
            .type<citm_performance>()
                .member("eventId",        &citm_performance::event_id)
                .member("id",             &citm_performance::id)
                .member("logo",           &citm_performance::logo)
                .member("name",           &citm_performance::name)
                .member("prices",         &citm_performance::prices)
                .member("seatCategories", &citm_performance::seat_categories)
                .member("seatMapImage",   &citm_performance::seat_map_image)
                .member("start",          &citm_performance::start)
                .member("venueCode",      &citm_performance::venue_code)
            .type<citm_catalog>()
                .member("performances", &citm_catalog::performances)
            .type<citm_performance_ids>()
                .member("eventId", &citm_performance_ids::event_id)
                .member("id",      &citm_performance_ids::id)
            .type<citm_catalog_ids>()
                .member("performances", &citm_catalog_ids::performances)
            .type<canada_geometry>()
                .member("coordinates", &canada_geometry::coordinates)
            .type<canada_feature>()
                .member("geometry", &canada_feature::geometry)
            .type<canada_collection>()
                .member("features", &canada_collection::features)
            .enum_type<ticket_state>("ticket_state",
                                     {
                                       { ticket_state::open,                          "open"                          },
                                       { ticket_state::closed,                        "closed"                        },
                                       { ticket_state::pending,                       "pending"                       },
                                       { ticket_state::duplicate,                     "duplicate"                     },
                                       { ticket_state::awaiting_customer_response,    "awaiting_customer_response"    },
                                       { ticket_state::escalated_to_engineering,      "escalated_to_engineering"      },
                                       { ticket_state::resolved_without_any_change,   "resolved_without_any_change"   },
                                       { ticket_state::waiting_on_third_party_vendor, "waiting_on_third_party_vendor" },
                                     }
                                    )
            .register_optional<std::optional<std::string>>()
            .register_container<std::vector<std::int64_t>>()
            .register_container<std::vector<citm_price>>()
            .register_container<std::vector<citm_area>>()
            .register_container<std::vector<citm_seat_category>>()
            .register_container<std::vector<citm_performance>>()
            .register_container<std::vector<citm_performance_ids>>()
            .register_container<std::vector<double>>()
            .register_container<std::vector<std::vector<double>>>()
            .register_container<std::vector<std::vector<std::vector<double>>>>()
            .register_container<std::vector<canada_feature>>()
            .register_container<std::vector<std::string>>()
            .register_container<std::vector<ticket_state>>()
        .compose_checked(formats::defaults())
        ;

    return instance;
}

/// What both strings cases decode to: 20000 URL-ish strings, each with a '/' and an 'é' in it.
const std::vector<std::string>& benchmark_strings()
{
    static const std::vector<std::string> instance = []
        {
            std::vector<std::string> out;
            for (std::size_t idx = 0U; idx < 20000U; ++idx)
            {
                auto id = std::to_string(idx);
                out.push_back("https://example.com/menu/" + id + "/caf\xC3\xA9-au-lait/r\xC3\xA9sum\xC3\xA9/" + id);
            }
            return out;
        }();

    return instance;
}

/// `benchmark_strings` as a JSON array. The parser only marks a string escaped if it holds a backslash, so the escaped
/// spelling writes every '/' as `\/` and every 'é' as `\u00e9`, which makes each element a `string_escaped` node; the
/// plain spelling writes both as they are, and each element is `string_canonical`.
///
/// The escaped document is about 30% larger, so some of the gap between the two `from_text` rows is scanning. The
/// `parse_then_extract` rows decode while parsing, and a reader over a `value` only produces canonical strings, so the
/// two `from_value` rows do the same work.
std::string synthesize_strings(bool escaped)
{
    std::string out = "[";
    for (const std::string& text : benchmark_strings())
    {
        out += out.size() == 1U ? "\"" : ",\"";
        for (std::size_t idx = 0U; idx < text.size(); ++idx)
        {
            if (escaped && text[idx] == '/')
            {
                out += "\\/";
            }
            else if (escaped && text.compare(idx, 2U, "\xC3\xA9") == 0)
            {
                out += "\\u00e9";
                ++idx;
            }
            else
            {
                out += text[idx];
            }
        }
        out += '"';
    }
    out += ']';
    return out;
}

/// What the enum case decodes to: 100000 ticket states, drawn from `std::minstd_rand` rather than taken in rotation so
/// that no branch predictor learns the sequence and flatters the lookup. The engine's output is fixed by the standard,
/// so every platform builds the same document.
const std::vector<ticket_state>& benchmark_ticket_states()
{
    static const std::vector<ticket_state> instance = []
        {
            std::minstd_rand          pick;
            std::vector<ticket_state> out;
            for (std::size_t idx = 0U; idx < 100000U; ++idx)
                out.push_back(static_cast<ticket_state>(pick() % 8U));
            return out;
        }();

    return instance;
}

/// `benchmark_ticket_states` as a JSON array of their spellings, 1.95 MB of `string_canonical` nodes.
std::string synthesize_ticket_states()
{
    // In the order of the enumerators, and the same spellings `extract_benchmark_formats` maps them from.
    static const char* const spellings[] = { "open",
                                             "closed",
                                             "pending",
                                             "duplicate",
                                             "awaiting_customer_response",
                                             "escalated_to_engineering",
                                             "resolved_without_any_change",
                                             "waiting_on_third_party_vendor",
                                           };

    std::string out = "[";
    for (ticket_state state : benchmark_ticket_states())
    {
        out += out.size() == 1U ? "\"" : ",\"";
        out += spellings[static_cast<std::size_t>(state)];
        out += '"';
    }
    out += ']';
    return out;
}

std::string load_citm_catalog()
{
    return benchmark_test_initializer::load_from_file(test_path("citm_catalog.json"));
}

std::string load_canada()
{
    return benchmark_test_initializer::load_from_file(test_path("canada.json"));
}

std::string load_canonical_strings()
{
    return synthesize_strings(false);
}

std::string load_escaped_strings()
{
    return synthesize_strings(true);
}

void check_citm_records(const citm_catalog& catalog)
{
    std::size_t  prices          = 0U;
    std::size_t  seat_categories = 0U;
    std::size_t  areas           = 0U;
    std::int64_t amount          = 0;
    for (const auto& performance : catalog.performances)
    {
        prices          += performance.prices.size();
        seat_categories += performance.seat_categories.size();
        for (const auto& price : performance.prices)
            amount += price.amount;
        for (const auto& category : performance.seat_categories)
            areas += category.areas.size();
    }

    ensure_eq(catalog.performances.size(), 243U);
    ensure_eq(prices,                      907U);
    ensure_eq(seat_categories,             907U);
    ensure_eq(areas,                       8685U);
    ensure_eq(amount,                      42356300);
}

void check_citm_sparse(const citm_catalog_ids& catalog)
{
    std::int64_t ids       = 0;
    std::int64_t event_ids = 0;
    for (const auto& performance : catalog.performances)
    {
        ids       += performance.id;
        event_ids += performance.event_id;
    }

    ensure_eq(catalog.performances.size(), 243U);
    ensure_eq(ids,                         52385309671);
    ensure_eq(event_ids,                   52183973487);
}

void check_canada(const canada_collection& collection)
{
    ensure_eq(collection.features.size(), 1U);

    const auto& rings  = collection.features[0].geometry.coordinates;
    std::size_t points = 0U;
    for (const auto& ring : rings)
    {
        points += ring.size();
        for (const auto& point : ring)
            ensure_eq(point.size(), 2U);
    }

    ensure_eq(rings.size(), 480U);
    ensure_eq(points,       55563U);
}

void check_strings(const std::vector<std::string>& strings)
{
    ensure_eq(strings.size(), benchmark_strings().size());
    ensure(strings == benchmark_strings());
}

void check_ticket_states(const std::vector<ticket_state>& states)
{
    ensure_eq(states.size(), benchmark_ticket_states().size());
    ensure(states == benchmark_ticket_states());
}

enum class extract_pipeline
{
    parse_then_extract,
    from_text,
    from_value,
};

template <typename T>
class extract_benchmark_test :
        public unit_test
{
public:
    using loader  = std::string (*)();
    using checker = void (*)(const T&);

public:
    extract_benchmark_test(const std::string& name, extract_pipeline pipeline, loader load, checker check) :
            unit_test("benchmark/extract/" + name),
            pipeline(pipeline),
            load(load),
            check(check)
    { }

    virtual void run_impl() override
    {
        const formats&    fmts   = extract_benchmark_formats();
        const std::string src    = load();
        const value       parsed = pipeline == extract_pipeline::from_value ? parse(src) : value();

        stopwatch timer;
        for (unsigned cnt = 0; cnt < extract_iterations; ++cnt)
        {
            // Outside the timed scope, so that the result is destroyed outside it too. The old pipeline's `value` is a
            // temporary and is torn down inside it, which is part of what that pipeline costs.
            T out{};
            {
                JSONV_TEST_TIME(timer);
                switch (pipeline)
                {
                case extract_pipeline::parse_then_extract:
                    out = extract<T>(parse(src), fmts);
                    break;
                case extract_pipeline::from_text:
                    // An lvalue, which the reader views where it is. A `std::string` rvalue would be moved into the
                    // reader and freed inside the timed scope.
                    out = extract<T>(src, fmts);
                    break;
                case extract_pipeline::from_value:
                    out = extract<T>(parsed, fmts);
                    break;
                }
            }
            check(out);
        }
        std::cout << timer.get();
    }

private:
    extract_pipeline pipeline;
    loader           load;
    checker          check;
};

class extract_benchmark_initializer
{
public:
    extract_benchmark_initializer()
    {
        add<citm_catalog>("citm_records", load_citm_catalog, check_citm_records);
        add<citm_catalog_ids>("citm_sparse", load_citm_catalog, check_citm_sparse);
        add<canada_collection>("canada_coordinates", load_canada, check_canada);
        add<std::vector<std::string>>("strings_canonical", load_canonical_strings, check_strings);
        add<std::vector<std::string>>("strings_escaped", load_escaped_strings, check_strings);
        add<std::vector<ticket_state>>("enum_strings", synthesize_ticket_states, check_ticket_states);
    }

private:
    template <typename T>
    void add(const std::string&                          name,
             typename extract_benchmark_test<T>::loader  load,
             typename extract_benchmark_test<T>::checker check
            )
    {
        static const std::pair<extract_pipeline, const char*> pipelines[] =
            {
                { extract_pipeline::parse_then_extract, "parse_then_extract" },
                { extract_pipeline::from_text,          "from_text"          },
                { extract_pipeline::from_value,         "from_value"         },
            };

        for (const auto& [pipeline, suffix] : pipelines)
            _tests.emplace_back(new extract_benchmark_test<T>(name + "/" + suffix, pipeline, load, check));
    }

private:
    std::deque<std::unique_ptr<unit_test>> _tests;
} extract_benchmark_initializer_instance;

}

}
