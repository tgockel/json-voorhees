/// \file
/// Tests for the composite adapters which read a \c reader directly: \c container_adapter, \c optional_adapter and
/// \c wrapper_adapter.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/ast.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization_builder.hpp>
#include <jsonv/serialization/function_extractor.hpp>
#include <jsonv/value.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <deque>
#include <limits>
#include <list>
#include <optional>
#include <stdexcept>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

/// Step a fresh reader off `document_start` and onto the value itself.
reader open(std::string_view source)
{
    reader out(source);
    (void) out.next_token();
    return out;
}

/// The same, over a tree rather than over text, so every case can be run against both sources. The two differ in what
/// the reader can lend, which is exactly what several of these adapters branch on.
reader open_value(const value& source)
{
    reader out = reader::from_value(source);
    (void) out.next_token();
    return out;
}

struct triple
{
    std::int64_t a;
    std::int64_t b;
    std::int64_t c;

    bool operator==(const triple& other) const
    {
        return a == other.a && b == other.b && c == other.c;
    }

    friend std::ostream& operator<<(std::ostream& os, const triple& x)
    {
        return os << "{ " << x.a << ", " << x.b << ", " << x.c << " }";
    }
};

/// A wrapper over a scalar -- explicitly convertible both ways, which is all \c wrapper_adapter asks for.
struct user_id
{
    using value_type = std::int64_t;

    std::int64_t raw;

    explicit user_id(std::int64_t raw = 0) : raw(raw) { }

    explicit operator std::int64_t() const { return raw; }

    bool operator==(const user_id& other) const { return raw == other.raw; }

    friend std::ostream& operator<<(std::ostream& os, const user_id& x) { return os << "user_id(" << x.raw << ")"; }
};

/// A wrapper over a structure, which is where a pass-through has something to get wrong about the cursor.
struct tags
{
    using value_type = std::vector<std::string>;

    std::vector<std::string> raw;

    explicit tags(std::vector<std::string> raw = {}) : raw(std::move(raw)) { }

    explicit operator std::vector<std::string>() const { return raw; }
};

/// A wrapper which rejects what it is handed, so construction throws once the inner extraction has already stepped
/// the cursor past the value.
struct positive
{
    using value_type = std::int64_t;

    std::int64_t raw;

    explicit positive(std::int64_t raw = 0) :
            raw(raw)
    {
        if (raw < 0)
            throw std::invalid_argument("must be positive");
    }

    explicit operator std::int64_t() const { return raw; }
};

/// The same, for an optional-like type: `std::optional` has no constructor which refuses, so a user-defined one is
/// what reaches `optional_adapter`'s construction failure path.
struct picky_optional
{
    using value_type = std::int64_t;

    std::optional<std::int64_t> held;

    picky_optional() = default;

    explicit picky_optional(std::int64_t raw)
    {
        if (raw < 0)
            throw std::invalid_argument("must be positive");

        held = raw;
    }

    explicit operator bool() const { return held.has_value(); }

    const std::int64_t& operator*() const { return *held; }
};

/// A comparator which throws, so `std::set::insert` fails after the element has been extracted -- a failure inside
/// the container's own loop rather than inside an element.
struct throwing_less
{
    bool operator()(std::int64_t a, std::int64_t b) const
    {
        if (a == 2 || b == 2 || a == 5 || b == 5)
            throw std::runtime_error("comparator refuses");

        return a < b;
    }
};

using refusing_set = std::set<std::int64_t, throwing_less>;

/// The same, over elements which are themselves structures -- which is what tells a resynchronising walk apart from
/// one that merely steps over the next token.
struct throwing_vector_less
{
    bool operator()(const std::vector<std::int64_t>& a, const std::vector<std::int64_t>& b) const
    {
        auto refused = [] (const std::vector<std::int64_t>& v)
                       {
                           return !v.empty() && (v.front() == 2 || v.front() == 5);
                       };

        if (refused(a) || refused(b))
            throw std::runtime_error("comparator refuses");

        return a < b;
    }
};

using refusing_vector_set = std::set<std::vector<std::int64_t>, throwing_vector_less>;

/// An optional-like type whose *default* constructor throws, which is the `null` branch's way of failing after the
/// cursor has moved. `std::optional` cannot reach it -- its default constructor is `noexcept`.
struct fussy_optional
{
    using value_type = std::int64_t;

    std::optional<std::int64_t> held;

    fussy_optional()
    {
        throw std::runtime_error("default construction refuses");
    }

    explicit fussy_optional(std::int64_t raw) :
            held(raw)
    { }

    explicit operator bool() const { return held.has_value(); }

    const std::int64_t& operator*() const { return *held; }
};

/// A container whose *move* constructor throws, so the failure happens after the closing token has been read and
/// there is nothing left to walk.
struct fragile
{
    using value_type = std::int64_t;

    std::vector<std::int64_t> raw;

    fragile()                          = default;
    fragile(const fragile&)            = default;
    fragile& operator=(const fragile&) = default;
    fragile& operator=(fragile&&)      = default;

    fragile(fragile&& other) :
            raw(std::move(other.raw))
    {
        if (std::find(raw.begin(), raw.end(), 9) != raw.end())
            throw std::runtime_error("move refuses");
    }

    auto begin() const { return raw.begin(); }
    auto end()   const { return raw.end(); }

    auto insert(std::vector<std::int64_t>::const_iterator at, std::int64_t value)
    {
        return raw.insert(at, value);
    }
};

/// Returned in place by a registered callable. Its move refuses, which is what the pipeline does to the callable's
/// result on the way into the `std::expected` it speaks -- after the callable has already consumed its value.
struct throwing_move_item
{
    std::int64_t raw = 0;

    throwing_move_item()                                     = default;
    throwing_move_item(const throwing_move_item&)            = default;
    throwing_move_item& operator=(const throwing_move_item&) = default;
    throwing_move_item& operator=(throwing_move_item&&)      = default;

    explicit throwing_move_item(std::int64_t raw) :
            raw(raw)
    { }

    throwing_move_item(throwing_move_item&& other) :
            raw(other.raw)
    {
        if (raw == 9)
            throw std::runtime_error("move refuses");
    }
};

/// Survives its first move and refuses every one after it. The first is the pipeline normalising the callable's
/// result, which happens while the `value` bridge still owns the outcome; the next is the one this is aimed at.
struct late_move_item
{
    std::int64_t raw   = 0;
    int          moves = 0;

    late_move_item()                                 = default;
    late_move_item(const late_move_item&)            = default;
    late_move_item& operator=(const late_move_item&) = default;
    late_move_item& operator=(late_move_item&&)      = default;

    explicit late_move_item(std::int64_t raw) :
            raw(raw)
    { }

    late_move_item(late_move_item&& other) :
            raw(other.raw),
            moves(other.moves + 1)
    {
        if (raw == 9 && moves >= 2)
            throw std::runtime_error("move refuses");
    }
};

/// A `const value&` callable, which is the registration form that reaches the bridge. It builds its result in place,
/// so the only moves are the pipeline's own.
const formats& post_commit_move_formats()
{
    static const formats instance =
        [] ()
        {
            static auto extractor =
                make_extractor([] (const value& from) -> std::expected<late_move_item, ast_node_type>
                               {
                                   return std::expected<late_move_item, ast_node_type>(std::in_place,
                                                                                       from.as_integer()
                                                                                      );
                               });

            formats out = formats::compose({ formats_builder()
                                                 .register_container<std::vector<late_move_item>>(),
                                             formats::defaults()
                                           });
            out.register_extractor(&extractor);
            return out;
        }();

    return instance;
}

/// A reader-shaped callable, which is the registration form that consumes its value before the pipeline touches the
/// result. `item` has no serializer, so this cannot go through `compose_checked`.
const formats& throwing_move_formats()
{
    static const formats instance =
        [] ()
        {
            static auto extractor =
                make_extractor([] (extraction_context& context, reader& from)
                                   -> std::expected<throwing_move_item, ast_node_type>
                               {
                                   auto raw = context.extract<std::int64_t>(from);
                                   if (!raw)
                                       return std::unexpected(raw.error());

                                   // In place, so the only move is the one the pipeline makes on the way out. A
                                   // `return throwing_move_item(*raw)` would convert -- and therefore move -- inside
                                   // the callable, which is a failure before the value is provably consumed and is
                                   // deliberately not treated as one.
                                   return std::expected<throwing_move_item, ast_node_type>(std::in_place, *raw);
                               });

            formats out = formats::compose({ formats_builder()
                                                 .register_container<std::vector<throwing_move_item>>(),
                                             formats::defaults()
                                           });
            out.register_extractor(&extractor);
            return out;
        }();

    return instance;
}

/// A `triple` whose `a` member is a container, so a failure inside it has both a member and an index to name.
struct holder
{
    std::vector<std::int64_t> a;
};

const formats& composite_formats()
{
    static const formats instance =
        formats_builder()
            .type<triple>()
                .member("a", &triple::a)
                .member("b", &triple::b)
                .member("c", &triple::c)
            .type<holder>()
                .member("a", &holder::a)
            .register_container<std::vector<std::int64_t>>()
            .register_container<std::vector<std::string>>()
            .register_container<std::vector<double>>()
            .register_container<std::vector<triple>>()
            .register_container<std::vector<std::vector<std::int64_t>>>()
            .register_container<std::list<std::string>>()
            .register_container<std::set<std::int64_t>>()
            .register_container<std::deque<double>>()
            .register_optional<std::optional<std::int64_t>>()
            .register_optional<std::optional<double>>()
            .register_optional<std::optional<triple>>()
            .register_wrapper<user_id>()
            .register_wrapper<tags>()
            .register_wrapper<positive>()
            .register_container<std::vector<positive>>()
            .register_optional<picky_optional>()
            .register_container<std::vector<picky_optional>>()
            .register_container<refusing_set>()
            .register_container<std::vector<refusing_set>>()
            .register_container<refusing_vector_set>()
            .register_container<std::vector<refusing_vector_set>>()
            .register_optional<fussy_optional>()
            .register_container<std::vector<fussy_optional>>()
            .register_container<fragile>()
            .register_container<std::vector<fragile>>()
        .compose_checked(formats::defaults())
        ;

    return instance;
}

extract_options collecting(extract_options::size_type max_failures = 10U)
{
    return extract_options::create_default()
                .failure_mode(extract_options::on_error::collect_all)
                .max_failures(max_failures);
}

/// Extract a \c T out of the middle of an array and report the token the reader is sitting on afterwards. Every
/// extractor owes its caller the same thing -- one position past the value it read -- and getting that wrong shows up
/// as a sibling consumed twice or not at all.
template <typename T>
std::string_view token_after_extracting(std::string_view source)
{
    extraction_context cxt(composite_formats());
    reader             rdr(source);

    (void) rdr.next_token();   // onto the `[`
    (void) rdr.next_token();   // onto the value to read

    auto out = cxt.extract<T>(rdr);
    ensure(out.has_value());
    ensure(rdr.good());
    return rdr.current().token_raw();
}

/// The problem an extraction of \c T from \a source must fail with, as the single entry it reports.
template <typename T>
extraction_error::problem refused(std::string_view source)
{
    extraction_context cxt(composite_formats());
    auto               rdr = open(source);

    auto out = cxt.extract<T>(rdr);
    ensure(!out.has_value());
    ensure_eq(1U, cxt.problems().size());
    return cxt.problems().at(0);
}

bool mentions(const extraction_error::problem& problem, std::string_view text)
{
    return problem.message().find(text) != std::string::npos;
}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// container_adapter                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(composite_container_reads_a_vector_of_scalars)
{
    for (bool from_value : { false, true })
    {
        const value        tree = parse(R"([ 1, 2, 3 ])");
        extraction_context cxt(composite_formats());
        auto               rdr = from_value ? open_value(tree) : open(R"([ 1, 2, 3 ])");

        auto out = cxt.extract<std::vector<std::int64_t>>(rdr);
        ensure(out.has_value());
        ensure_eq(3U, out->size());
        ensure_eq(1, out->at(0));
        ensure_eq(2, out->at(1));
        ensure_eq(3, out->at(2));
        ensure(cxt.problems().empty());
    }
}

TEST(composite_container_reads_an_empty_array)
{
    for (bool from_value : { false, true })
    {
        const value        tree = parse("[]");
        extraction_context cxt(composite_formats());
        auto               rdr = from_value ? open_value(tree) : open("[]");

        auto out = cxt.extract<std::vector<std::int64_t>>(rdr);
        ensure(out.has_value());
        ensure(out->empty());
        ensure(cxt.problems().empty());
    }
}

TEST(composite_container_reads_containers_without_reserve)
{
    // `std::list`, `std::set` and `std::deque` have nothing to reserve, which is the arm of `reserve_if_possible`
    // that has to compile away rather than fail to compile.
    extraction_context cxt(composite_formats());

    auto strings = [&] { auto r = open(R"([ "a", "b" ])");  return cxt.extract<std::list<std::string>>(r);  }();
    ensure(strings.has_value());
    ensure_eq(2U, strings->size());
    ensure_eq(std::string("a"), strings->front());
    ensure_eq(std::string("b"), strings->back());

    auto ints = [&] { auto r = open(R"([ 3, 1, 2, 1 ])"); return cxt.extract<std::set<std::int64_t>>(r); }();
    ensure(ints.has_value());
    ensure_eq(3U, ints->size());
    ensure(ints->count(1) == 1U);
    ensure(ints->count(3) == 1U);

    auto decimals = [&] { auto r = open(R"([ 1.5, 2.5 ])"); return cxt.extract<std::deque<double>>(r); }();
    ensure(decimals.has_value());
    ensure_eq(2U, decimals->size());
    ensure_eq(1.5, decimals->at(0));
    ensure_eq(2.5, decimals->at(1));

    ensure(cxt.problems().empty());
}

TEST(composite_container_reads_a_vector_of_a_user_type)
{
    for (bool from_value : { false, true })
    {
        const value        tree = parse(R"([ { "a": 1, "b": 2, "c": 3 }, { "a": 4, "b": 5, "c": 6 } ])");
        extraction_context cxt(composite_formats());
        auto               rdr = from_value
                               ? open_value(tree)
                               : open(R"([ { "a": 1, "b": 2, "c": 3 }, { "a": 4, "b": 5, "c": 6 } ])");

        auto out = cxt.extract<std::vector<triple>>(rdr);
        ensure(out.has_value());
        ensure_eq(2U, out->size());
        ensure_eq((triple{ 1, 2, 3 }), out->at(0));
        ensure_eq((triple{ 4, 5, 6 }), out->at(1));
        ensure(cxt.problems().empty());
    }
}

TEST(composite_container_lands_one_past_the_array)
{
    // The cursor contract, which is where a nested container mistake surfaces as a sibling consumed twice or skipped.
    ensure_eq(std::string_view("3"), token_after_extracting<std::vector<std::int64_t>>(R"([ [ 1, 2 ], 3 ])"));
    ensure_eq(std::string_view("3"), token_after_extracting<std::vector<std::int64_t>>(R"([ [], 3 ])"));
    ensure_eq(std::string_view("3"), token_after_extracting<std::vector<triple>>(
                                         R"([ [ { "a": 1, "b": 2, "c": 3 } ], 3 ])"
                                     ));
}

TEST(composite_nested_containers_walk_without_double_consuming)
{
    for (bool from_value : { false, true })
    {
        const value        tree = parse(R"([ [ 1, 2 ], [], [ 3 ] ])");
        extraction_context cxt(composite_formats());
        auto               rdr = from_value ? open_value(tree) : open(R"([ [ 1, 2 ], [], [ 3 ] ])");

        auto out = cxt.extract<std::vector<std::vector<std::int64_t>>>(rdr);
        ensure(out.has_value());
        ensure_eq(3U, out->size());
        ensure_eq(2U, out->at(0).size());
        ensure_eq(1, out->at(0).at(0));
        ensure_eq(2, out->at(0).at(1));
        ensure(out->at(1).empty());
        ensure_eq(1U, out->at(2).size());
        ensure_eq(3, out->at(2).at(0));
        ensure(cxt.problems().empty());
    }
}

TEST(composite_container_of_a_non_array_is_refused)
{
    struct
    {
        std::string_view source;
        ast_node_type    found;
    }
    const cases[] =
    {
        { "5",       ast_node_type::integer          },
        { "{ }",     ast_node_type::object_begin     },
        { R"("x")",  ast_node_type::string_canonical },
        { "null",    ast_node_type::literal_null     },
    };

    for (const auto& c : cases)
    {
        extraction_context cxt(composite_formats());
        auto               rdr = open(c.source);

        auto out = cxt.extract<std::vector<std::int64_t>>(rdr);
        ensure(!out.has_value());

        // The error channel carries the type actually found rather than the `error` sentinel, so a caller can branch
        // on what was really there.
        ensure(out.error() == c.found);
        ensure_eq(1U, cxt.problems().size());
        ensure(mentions(cxt.problems().at(0), "when expecting array"));
    }
}

TEST(composite_container_names_the_failing_element)
{
    // `.a[1]` rather than `.a`: the member names the container and the element scope names the index inside it.
    auto problem = refused<holder>(R"({ "a": [ 1, "x", 3 ] })");

    ensure_eq(path::create(".a[1]"), problem.path());
    ensure(mentions(problem, "when expecting integer"));
}

TEST(composite_container_from_a_truncated_document)
{
    // `parse_index::parse` hands back a usable index for a failed parse, ending it with an `error` node. The element
    // loop meets that node where the rest of the array should have been.
    for (std::string_view source : { R"([ 1, 2)", R"([)" })
    {
        extraction_context cxt(composite_formats());
        auto               rdr = open(source);

        auto out = cxt.extract<std::vector<std::int64_t>>(rdr);
        ensure(!out.has_value());
        ensure(!cxt.problems().empty());
        ensure(mentions(cxt.problems().at(0), "Unterminated array"));
    }
}

TEST(composite_container_reserves_from_the_element_count)
{
    // The opening token carries the count, so the vector is sized once. Asked of the result rather than of an
    // allocation counter so that it means the same thing under the sanitizers, where counting is compiled out.
    constexpr std::size_t count = 1024U;

    std::string source = "[";
    for (std::size_t idx = 0U; idx < count; ++idx)
        source += (idx == 0U ? "1" : ",1");
    source += "]";

    extraction_context cxt(composite_formats());
    auto               rdr = open(source);

    auto out = cxt.extract<std::vector<std::int64_t>>(rdr);
    ensure(out.has_value());
    ensure_eq(count, out->size());

    // Exactly the count, which only holds if nothing grew its way there.
    ensure_eq(count, out->capacity());
}

TEST(composite_container_collect_all_resumes_after_a_nested_container)
{
    // A container which recovered still reports failure, having consumed the whole of itself on the way. The
    // enclosing loop has to resume at the *next* array rather than the one after it, or `[2]` goes unreported and
    // `[1]` is never read.
    for (bool from_value : { false, true })
    {
        const std::string_view text = R"([ [ 1, "x" ], [ 2 ], [ 3, "y" ] ])";
        const value            tree = parse(text);

        extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = from_value ? open_value(tree) : open(text);

        ensure(!cxt.extract<std::vector<std::vector<std::int64_t>>>(rdr).has_value());
        ensure_eq(2U, cxt.problems().size());
        ensure_eq(path::create("[0][1]"), cxt.problems().at(0).path());
        ensure_eq(path::create("[2][1]"), cxt.problems().at(1).path());
    }
}


TEST(composite_construction_failure_does_not_swallow_the_next_element)
{
    // A wrapper or optional whose constructor rejects the value fails *after* the inner extraction stepped the
    // cursor past it. Recovering by stepping again would skip the following element -- losing it, losing whatever it
    // had to report, and renumbering the rest.
    for (bool optional_like : { false, true })
    {
        extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open("[ -1, -2, 5 ]");

        if (optional_like)
            ensure(!cxt.extract<std::vector<picky_optional>>(rdr).has_value());
        else
            ensure(!cxt.extract<std::vector<positive>>(rdr).has_value());

        ensure_eq(2U, cxt.problems().size());
        ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
        ensure_eq(path::create("[1]"), cxt.problems().at(1).path());
    }
}

TEST(composite_construction_failure_on_the_last_element_does_not_invent_a_problem)
{
    // The same step, taken at the end of the array, used to walk off the `]` and leave the loop reporting an
    // unterminated array on top of the real failure.
    extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
    auto               rdr = open("[ 5, -1 ]");

    ensure(!cxt.extract<std::vector<positive>>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create("[1]"), cxt.problems().at(0).path());
}

TEST(composite_insertion_failure_does_not_end_the_enclosing_array)
{
    // An insertion which throws leaves the cursor inside the array being built, which means nothing to the loop
    // above it: resuming there reads the inner `]` as the *outer* array's end and stops, so the second failure is
    // never seen. The container finishes walking its own array before letting the failure out.
    extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
    auto               rdr = open("[ [ 1, 2, 3 ], [ 4, 5 ] ]");

    ensure(!cxt.extract<std::vector<refusing_set>>(rdr).has_value());
    ensure_eq(2U, cxt.problems().size());
    ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
    ensure_eq(path::create("[1]"), cxt.problems().at(1).path());
}


TEST(composite_insertion_failure_with_a_structured_tail_finds_its_own_end)
{
    // The elements after the failure are arrays. Resynchronising by leaving "the current structure" would leave one
    // of *those* and land back inside the container being built, so the loop above would read its `]` as the outer
    // array's end and stop after one problem. Stepping over whole child values is what finds the right closing
    // token.
    extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
    auto               rdr = open("[ [ [1], [2], [3] ], [ [4], [5] ] ]");

    ensure(!cxt.extract<std::vector<refusing_vector_set>>(rdr).has_value());
    ensure_eq(2U, cxt.problems().size());
    ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
    ensure_eq(path::create("[1]"), cxt.problems().at(1).path());
}

TEST(composite_optional_default_construction_failure_does_not_swallow_the_next_element)
{
    // The `null` branch steps the cursor before it builds anything, so an optional-like type which refuses to
    // default-construct fails with the value behind it just as the converting constructor does.
    {
        extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open("[ null, null, 5 ]");

        ensure(!cxt.extract<std::vector<fussy_optional>>(rdr).has_value());
        ensure_eq(2U, cxt.problems().size());
        ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
        ensure_eq(path::create("[1]"), cxt.problems().at(1).path());
    }

    {
        extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open("[ 5, null ]");

        ensure(!cxt.extract<std::vector<fussy_optional>>(rdr).has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_eq(path::create("[1]"), cxt.problems().at(0).path());
    }
}

TEST(composite_container_move_failure_keeps_the_following_diagnostics)
{
    // Moving the built container into the result fails *after* its closing token has been read, so there is nothing
    // left to walk -- resynchronising anyway would consume the following sibling and lose what it had to report.
    {
        extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open(R"([ [ 9 ], [ "y" ] ])");

        ensure(!cxt.extract<std::vector<fragile>>(rdr).has_value());
        ensure_eq(2U, cxt.problems().size());
        ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
        ensure_eq(path::create("[1][0]"), cxt.problems().at(1).path());
    }

    {
        extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open("[ [ 1 ], [ 9 ] ]");

        ensure(!cxt.extract<std::vector<fragile>>(rdr).has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_eq(path::create("[1]"), cxt.problems().at(0).path());
    }
}

TEST(composite_construction_failure_names_the_enclosing_structure)
{
    // With no `path_scope` above it there is nothing but the reader to ask, and by the time construction fails the
    // cursor names the *next* sibling. Reporting that would blame a value which extracted perfectly well, so the
    // enclosing structure is reported instead -- the same approximation the `value` bridge makes, and for the same
    // reason: naming the position exactly would mean building a path before every successful extraction.
    extraction_context cxt(composite_formats());
    auto               rdr = open("[ -1, 2 ]");
    (void) rdr.next_token();   // onto `-1`, which is `[0]`

    ensure(!cxt.extract<positive>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(jsonv::path(), cxt.problems().at(0).path());
}


TEST(composite_callable_result_move_failure_keeps_the_following_diagnostics)
{
    // A registered callable which reads the reader has consumed its value by the time it returns, so moving what it
    // produced into the pipeline's `std::expected` fails with that value behind the cursor. Stepping over it again
    // would lose the next element's own failure, and at the end of the array invent one.
    {
        extraction_context cxt(throwing_move_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open(R"([ 9, "x" ])");

        ensure(!cxt.extract<std::vector<throwing_move_item>>(rdr).has_value());
        ensure_eq(2U, cxt.problems().size());
        ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
        ensure_eq(path::create("[1]"), cxt.problems().at(1).path());
        ensure(mentions(cxt.problems().at(1), "when expecting integer"));
    }

    {
        extraction_context cxt(throwing_move_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open("[ 1, 9 ]");

        ensure(!cxt.extract<std::vector<throwing_move_item>>(rdr).has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_eq(path::create("[1]"), cxt.problems().at(0).path());
    }
}


TEST(composite_post_commit_move_failure_keeps_the_following_diagnostics)
{
    // The `value` bridge commits -- stepping the cursor past the value -- and *then* the named return moves the
    // result out with the caller's own move constructor. The bridge cannot report that one for us: committing is
    // exactly the state it takes to mean the body succeeded, so the failure has to say so itself.
    //
    // This is coupled to how many moves the pipeline makes: `late_move_item` survives the first (the pipeline
    // normalising the callable's result, which the bridge still owns) and refuses the rest. If a toolchain elides
    // the return move entirely, the next move is the container's and this reports one problem instead of two.
    {
        extraction_context cxt(post_commit_move_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open(R"([ 9, "x" ])");

        ensure(!cxt.extract<std::vector<late_move_item>>(rdr).has_value());
        ensure_eq(2U, cxt.problems().size());
        ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
        ensure_eq(path::create("[1]"), cxt.problems().at(1).path());
    }

    {
        extraction_context cxt(post_commit_move_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open("[ 1, 9 ]");

        ensure(!cxt.extract<std::vector<late_move_item>>(rdr).has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_eq(path::create("[1]"), cxt.problems().at(0).path());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// optional_adapter                                                                                                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(composite_optional_reads_null_a_value_and_an_object)
{
    for (bool from_value : { false, true })
    {
        extraction_context cxt(composite_formats());

        const value none_tree = parse("null");
        auto        none_rdr  = from_value ? open_value(none_tree) : open("null");
        auto        none      = cxt.extract<std::optional<std::int64_t>>(none_rdr);
        ensure(none.has_value());
        ensure(!none->has_value());

        const value some_tree = parse("5");
        auto        some_rdr  = from_value ? open_value(some_tree) : open("5");
        auto        some      = cxt.extract<std::optional<std::int64_t>>(some_rdr);
        ensure(some.has_value());
        ensure(some->has_value());
        ensure_eq(5, **some);

        const value obj_tree = parse(R"({ "a": 1, "b": 2, "c": 3 })");
        auto        obj_rdr  = from_value ? open_value(obj_tree) : open(R"({ "a": 1, "b": 2, "c": 3 })");
        auto        obj      = cxt.extract<std::optional<triple>>(obj_rdr);
        ensure(obj.has_value());
        ensure(obj->has_value());
        ensure_eq((triple{ 1, 2, 3 }), **obj);

        ensure(cxt.problems().empty());
    }
}

TEST(composite_optional_lands_one_past_the_value)
{
    ensure_eq(std::string_view("2"), token_after_extracting<std::optional<std::int64_t>>(R"([ null, 2 ])"));
    ensure_eq(std::string_view("2"), token_after_extracting<std::optional<std::int64_t>>(R"([ 5, 2 ])"));
    ensure_eq(std::string_view("2"), token_after_extracting<std::optional<triple>>(
                                         R"([ { "a": 1, "b": 2, "c": 3 }, 2 ])"
                                     ));
}

TEST(composite_optional_non_finite_decimal_is_not_none)
{
    // A value-backed reader has no token for a non-finite `kind::decimal` and renders it as `null` -- what encoding
    // the value writes, not what the tree holds. Going by the node type alone would turn a NaN into an empty
    // optional, so the `value` is asked where there is one to ask.
    extraction_context cxt(composite_formats());

    for (double number : { std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity()
                         })
    {
        const value tree = value(number);
        auto        rdr  = open_value(tree);

        auto out = cxt.extract<std::optional<double>>(rdr);
        ensure(out.has_value());
        ensure(out->has_value());

        if (std::isnan(number))
            ensure(std::isnan(**out));
        else
            ensure_eq(number, **out);
    }

    // A real null is still none.
    const value null_tree = value();
    auto        null_rdr  = open_value(null_tree);
    auto        none      = cxt.extract<std::optional<double>>(null_rdr);
    ensure(none.has_value());
    ensure(!none->has_value());

    ensure(cxt.problems().empty());
}

TEST(composite_optional_of_a_bad_value_is_refused)
{
    auto problem = refused<std::optional<std::int64_t>>(R"("x")");
    ensure(mentions(problem, "when expecting integer"));

    // And nested, where the element scope names where it was.
    extraction_context cxt(composite_formats());
    auto               rdr = open(R"([ null, "x" ])");

    auto out = cxt.extract<std::vector<std::int64_t>>(rdr);
    ensure(!out.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// wrapper_adapter                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(composite_wrapper_round_trips)
{
    for (bool from_value : { false, true })
    {
        const value        tree = parse("7");
        extraction_context cxt(composite_formats());
        auto               rdr = from_value ? open_value(tree) : open("7");

        auto out = cxt.extract<user_id>(rdr);
        ensure(out.has_value());
        ensure_eq(user_id(7), *out);
        ensure(cxt.problems().empty());
    }

    ensure_eq(value(7), to_json(user_id(7), composite_formats()));
}

TEST(composite_wrapper_lands_one_past_the_value)
{
    ensure_eq(std::string_view("2"), token_after_extracting<user_id>(R"([ 7, 2 ])"));

    // A wrapper over a structure is where a pass-through has something to get wrong: the cursor contract is whatever
    // the wrapped type's extractor honours, and this one reads a whole array.
    ensure_eq(std::string_view("2"), token_after_extracting<tags>(R"([ [ "a", "b" ], 2 ])"));
}

TEST(composite_wrapper_of_a_container_passes_through)
{
    extraction_context cxt(composite_formats());
    auto               rdr = open(R"([ "a", "b" ])");

    auto out = cxt.extract<tags>(rdr);
    ensure(out.has_value());
    ensure_eq(2U, out->raw.size());
    ensure_eq(std::string("a"), out->raw.at(0));
    ensure_eq(std::string("b"), out->raw.at(1));
    ensure(cxt.problems().empty());
}

TEST(composite_wrapper_failure_passes_the_inner_diagnostic_through)
{
    // Nothing of the wrapper's own type appears in the document, so the failure it reports is the wrapped type's --
    // message, path and all.
    auto problem = refused<user_id>(R"("x")");
    ensure(mentions(problem, "when expecting integer"));

    auto nested = refused<tags>(R"([ "a", 5 ])");
    ensure_eq(path::create("[1]"), nested.path());
    ensure(mentions(nested, "when expecting string"));
}

}
