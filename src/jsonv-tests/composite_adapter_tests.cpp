/// \file
/// Tests for the composite adapters which read a \c reader directly: \c container_adapter, \c optional_adapter,
/// \c wrapper_adapter and \c polymorphic_adapter.
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
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
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

/// The base of a hierarchy read through \c polymorphic_adapter. Every subtype but \c polygon is told apart by its
/// `"kind"` member.
struct shape
{
    virtual ~shape() noexcept = default;

    virtual std::string_view name() const = 0;
};

struct circle final :
        shape
{
    std::int64_t radius = 0;

    std::string_view name() const override { return "circle"; }
};

struct square final :
        shape
{
    std::int64_t side = 0;

    std::string_view name() const override { return "square"; }
};

/// A subtype whose member is a view of the document, which it can only be if nothing materialised the object first.
struct label final :
        shape
{
    std::string_view text;

    std::string_view name() const override { return "label"; }
};

/// The one subtype with no `"kind"`, so the only one a discriminator has to see the whole value to pick.
struct polygon final :
        shape
{
    std::int64_t sides = 0;

    std::string_view name() const override { return "polygon"; }
};

/// A subtype whose move survives the extraction which builds it and refuses the one after, which is \c extract handing
/// the built object back to \c polymorphic_adapter -- after the cursor has stepped past what it was built from.
struct late_move_shape final :
        shape
{
    std::int64_t raw   = 0;
    int          moves = 0;

    explicit late_move_shape(std::int64_t raw) :
            raw(raw)
    { }

    late_move_shape(late_move_shape&& other) :
            shape(),
            raw(other.raw),
            moves(other.moves + 1)
    {
        if (raw == 9 && moves >= 3)
            throw std::runtime_error("move refuses");
    }

    std::string_view name() const override { return "late_move_shape"; }
};

/// Two subtypes which bind the discriminator as a member, so each knows what the document said it was.
struct plain_tag final :
        shape
{
    std::string kind;

    std::string_view name() const override { return "plain"; }
};

struct other_tag final :
        shape
{
    std::string kind;

    std::string_view name() const override { return "other"; }
};

/// Two subtypes whose discriminator is an object, bound whole as a \c value member -- which the built-in \c value
/// extractor materialises, rather than the DSL walking it.
struct first_nested final :
        shape
{
    value kind;

    std::string_view name() const override { return "first_nested"; }
};

struct second_nested final :
        shape
{
    value kind;

    std::string_view name() const override { return "second_nested"; }
};

/// How many times the discriminator for \c polygon has been asked. It is registered last, so a document any keyed
/// subtype matches should never reach it.
std::size_t polygon_discriminator_calls = 0U;

/// Name the shape \a extracted holds, or say why there is none.
std::string_view name_of(const std::expected<std::unique_ptr<shape>, ast_node_type>& extracted)
{
    if (!extracted)
        return "<failed>";
    else if (!*extracted)
        return "<null>";
    else
        return (*extracted)->name();
}

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
            .type<circle>()
                .member("radius", &circle::radius)
            .type<square>()
                .member("side", &square::side)
            .type<label>()
                .member("text", &label::text)
            .type<polygon>()
                .member("sides", &polygon::sides)
            .polymorphic_type<std::unique_ptr<shape>>("kind")
                .check_null_input()
                .subtype<circle>("circle")
                .subtype<square>("square")
                .subtype<label>("label")
                .subtype<polygon>([] (const value& from)
                                  {
                                      ++polygon_discriminator_calls;
                                      return from.is_object() && from.count("sides") != 0U;
                                  }
                                 )
            .register_container<std::vector<std::unique_ptr<shape>>>()
            // The same subtypes registered in the other order: a discriminator which reads the whole value ahead of
            // one which reads a single member.
            .polymorphic_type<std::shared_ptr<shape>>("kind")
                .subtype<square>([] (const value& from) { return from.is_object() && from.count("side") != 0U; })
                .subtype<circle>("circle")
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// polymorphic_adapter                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(composite_polymorphic_finds_the_discriminator_wherever_it_is)
{
    // A single forward pass would have to know the type before reading the first member. Reading ahead for the key
    // means where the document puts it makes no difference.
    for (std::string_view text : { R"({ "kind": "square", "side": 4, "extra": [ 1, { "kind": "circle" } ] })",
                                   R"({ "side": 4, "kind": "square", "extra": [ 1, { "kind": "circle" } ] })",
                                   R"({ "side": 4, "extra": [ 1, { "kind": "circle" } ], "kind": "square" })",
                                 }
        )
    {
        const value tree = parse(text);
        for (bool from_value : { false, true })
        {
            extraction_context cxt(composite_formats());
            auto               rdr = from_value ? open_value(tree) : open(text);

            auto out = cxt.extract<std::unique_ptr<shape>>(rdr);
            ensure_eq(std::string_view("square"), name_of(out));
            ensure_eq(4, dynamic_cast<const square&>(**out).side);
            ensure(cxt.problems().empty());
        }
    }
}

TEST(composite_polymorphic_general_discriminator_sees_the_whole_value)
{
    const std::string_view text = R"({ "sides": 5 })";
    const value            tree = parse(text);

    for (bool from_value : { false, true })
    {
        polygon_discriminator_calls = 0U;

        extraction_context cxt(composite_formats());
        auto               rdr = from_value ? open_value(tree) : open(text);

        auto out = cxt.extract<std::unique_ptr<shape>>(rdr);
        ensure_eq(std::string_view("polygon"), name_of(out));
        ensure_eq(5, dynamic_cast<const polygon&>(**out).sides);
        ensure_eq(1U, polygon_discriminator_calls);
    }
}

TEST(composite_polymorphic_keeps_registration_order)
{
    // Both discriminators match. Whichever was registered first wins, whether it is the one reading a single member
    // or the one reading the whole value.
    const std::string_view text = R"({ "kind": "circle", "radius": 2, "side": 3 })";
    const value            tree = parse(text);

    for (bool from_value : { false, true })
    {
        extraction_context unique_cxt(composite_formats());
        auto               unique_rdr = from_value ? open_value(tree) : open(text);
        ensure_eq(std::string_view("circle"), name_of(unique_cxt.extract<std::unique_ptr<shape>>(unique_rdr)));

        extraction_context shared_cxt(composite_formats());
        auto               shared_rdr = from_value ? open_value(tree) : open(text);
        auto               shared     = shared_cxt.extract<std::shared_ptr<shape>>(shared_rdr);
        ensure(shared.has_value());
        ensure_eq(std::string_view("square"), (*shared)->name());
    }
}

TEST(composite_polymorphic_discriminator_matches_like_parse)
{
    // An escaped key is the key it decodes to, and a duplicated one keeps its last value -- which is what `parse`
    // does with both, so a document means the same thing whichever way it is read.
    for (std::string_view text : { R"({ "kind": "square", "side": 4 })",
                                   R"({ "kind": "circle", "radius": 1, "side": 4, "kind": "square" })",
                                 }
        )
    {
        const value tree = parse(text);
        for (bool from_value : { false, true })
        {
            extraction_context cxt(composite_formats());
            auto               rdr = from_value ? open_value(tree) : open(text);
            ensure_eq(std::string_view("square"), name_of(cxt.extract<std::unique_ptr<shape>>(rdr)));
        }
    }
}

TEST(composite_polymorphic_null_input_is_an_empty_pointer)
{
    const value tree = parse("null");
    for (bool from_value : { false, true })
    {
        extraction_context cxt(composite_formats());
        auto               rdr = from_value ? open_value(tree) : open("null");
        ensure_eq(std::string_view("<null>"), name_of(cxt.extract<std::unique_ptr<shape>>(rdr)));
    }
}

TEST(composite_polymorphic_lands_one_past_the_value)
{
    ensure_eq(std::string_view("7"),
              token_after_extracting<std::unique_ptr<shape>>(R"([ { "radius": 1, "kind": "circle" }, 7 ])"));
    ensure_eq(std::string_view("7"), token_after_extracting<std::unique_ptr<shape>>(R"([ { "sides": 3 }, 7 ])"));
    ensure_eq(std::string_view("7"), token_after_extracting<std::unique_ptr<shape>>(R"([ null, 7 ])"));
}

TEST(composite_polymorphic_no_match_names_the_value)
{
    auto problem = refused<std::unique_ptr<shape>>(R"({ "kind": "hexagon" })");
    ensure_eq(path(), problem.path());
    ensure(mentions(problem, R"(No discriminators matched JSON value: {"kind":"hexagon"})"));

    auto nested = refused<std::vector<std::unique_ptr<shape>>>(R"([ { "kind": "circle", "radius": 1 }, 5 ])");
    ensure_eq(path::create("[1]"), nested.path());
    ensure(mentions(nested, "No discriminators matched JSON value: 5"));
}

TEST(composite_polymorphic_no_match_describes_what_it_cannot_render)
{
    // Rendering the value for the message means reading all of it, which `1e400` refuses. That is no reason to report
    // something other than what actually went wrong. Every discriminator here is keyed -- one which reads the whole
    // value would have to be shown the `1e400` to answer, and could not be.
    formats fmts = formats_builder()
                       .type<circle>()
                           .member("radius", &circle::radius)
                       .polymorphic_type<std::unique_ptr<shape>>("kind")
                           .subtype<circle>("circle")
                   .compose_checked(formats::defaults());

    extraction_context cxt(fmts);
    auto               rdr = open(R"({ "kind": "hexagon", "size": 1e400 })");

    ensure(!cxt.extract<std::unique_ptr<shape>>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("No discriminators matched JSON value"), cxt.problems().at(0).message());

    // And the cursor is still on the object, for whatever recovers from it.
    ensure(rdr.current_type() == ast_node_type::object_begin);
}

TEST(composite_polymorphic_subtype_failure_is_placed_in_the_document)
{
    const std::string_view text = R"([ { "kind": "circle", "radius": 1 }, { "kind": "circle", "radius": "x" } ])";
    const value            tree = parse(text);

    for (bool from_value : { false, true })
    {
        extraction_context cxt(composite_formats());
        auto               rdr = from_value ? open_value(tree) : open(text);

        ensure(!cxt.extract<std::vector<std::unique_ptr<shape>>>(rdr).has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_eq(path::create("[1].radius"), cxt.problems().at(0).path());
        ensure(mentions(cxt.problems().at(0), "when expecting integer"));
    }
}

TEST(composite_polymorphic_collect_all_resumes_after_an_unmatched_element)
{
    // An unmatched element is refused with the cursor still on it, so the container steps over it and no further.
    // Stepping one too far would drop the circle and with it the third element's problem.
    const std::string_view text = R"([ { "kind": "hexagon" }, { "kind": "circle", "radius": 1 }, { "kind": "nonagon" } ])";
    const value            tree = parse(text);

    for (bool from_value : { false, true })
    {
        extraction_context cxt(composite_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = from_value ? open_value(tree) : open(text);

        ensure(!cxt.extract<std::vector<std::unique_ptr<shape>>>(rdr).has_value());
        ensure_eq(2U, cxt.problems().size());
        ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
        ensure_eq(path::create("[2]"), cxt.problems().at(1).path());
        ensure(mentions(cxt.problems().at(1), "nonagon"));
    }
}

TEST(composite_polymorphic_keyed_match_does_not_read_the_rest_of_the_object)
{
    // `1e400` has no `double`, so reading the object whole fails -- which the value bridge did, before choosing a
    // subtype, for every member whether the subtype wanted it or not. Finding the discriminator steps over the blob
    // instead, and so does the circle, which has no member by that name. The discriminator which would need the whole
    // value is registered after the one which matches, so it is never asked.
    std::string text = R"({ "blob": [ )";
    for (int idx = 0; idx < 10000; ++idx)
        text += std::to_string(idx) + ", ";
    text += R"(1e400 ], "radius": 3, "kind": "circle" })";

    polygon_discriminator_calls = 0U;

    extraction_context cxt(composite_formats());
    auto               rdr = open(text);

    auto out = cxt.extract<std::unique_ptr<shape>>(rdr);
    ensure_eq(std::string_view("circle"), name_of(out));
    ensure_eq(3, dynamic_cast<const circle&>(**out).radius);
    ensure_eq(0U, polygon_discriminator_calls);
    ensure(!rdr.good() || rdr.current_type() == ast_node_type::document_end);
}

TEST(composite_polymorphic_keyed_subtype_views_the_source)
{
    // Nothing is materialised on the way to a keyed subtype, so it reads the document itself -- and a view it holds
    // names the document rather than a copy that dies before extraction returns.
    const std::string source = R"({ "kind": "label", "text": "a long string value which is not copied anywhere" })";

    extraction_context cxt(composite_formats());
    auto               rdr = open(source);

    auto out = cxt.extract<std::unique_ptr<shape>>(rdr);
    ensure(cxt.problems().empty());
    ensure_eq(std::string_view("label"), name_of(out));

    const auto& text = dynamic_cast<const label&>(**out).text;
    ensure_eq(std::string_view("a long string value which is not copied anywhere"), text);
    ensure(text.data() >= source.data() && text.data() < source.data() + source.size());
}

TEST(composite_polymorphic_discriminator_cannot_keep_a_view_of_its_copy)
{
    // From text, a general discriminator is shown a copy built for it, which dies before extraction returns -- so a
    // view of it is refused, as it was when the bridge built that copy. From a `value` it is the caller's own tree,
    // which a view may name. Either way the subtype it chooses reads the document, and views that.
    std::optional<bool> viewed;

    formats fmts = formats_builder()
                       .type<label>()
                           .member("text", &label::text)
                       .polymorphic_type<std::unique_ptr<shape>>()
                           .subtype<label>([&viewed] (extraction_context& context, const value& from)
                                           {
                                               try
                                               {
                                                   (void) context.extract<std::string_view>(from.at("text"));
                                                   viewed = true;
                                               }
                                               catch (const extraction_error&)
                                               {
                                                   viewed = false;
                                               }
                                               return true;
                                           }
                                          )
                   .compose_checked(formats::defaults());

    const std::string source = R"({ "text": "a long string value which is not copied anywhere" })";
    const value       tree   = parse(source);

    for (bool from_value : { false, true })
    {
        viewed.reset();

        extraction_context cxt(fmts);
        auto               rdr = from_value ? open_value(tree) : open(source);

        auto out = cxt.extract<std::unique_ptr<shape>>(rdr);
        ensure(cxt.problems().empty());
        ensure_eq(std::string_view("label"), name_of(out));
        ensure(viewed.has_value());
        ensure_eq(from_value, *viewed);

        const auto& text  = dynamic_cast<const label&>(**out).text;
        const auto& owner = from_value ? tree.at("text").as_string() : source;
        ensure(text.data() >= owner.data() && text.data() < owner.data() + owner.size());
    }
}

TEST(composite_polymorphic_subtype_move_failure_keeps_the_following_diagnostics)
{
    // The subtype is built and the cursor is past it before its move refuses, so whatever recovers has to resume at
    // the next element rather than step over it -- which would lose the circle's problem, or, at the end of the
    // array, invent an `Unterminated array`.
    static auto extractor =
        make_extractor([] (extraction_context& context, reader& from) -> std::expected<late_move_shape, ast_node_type>
                       {
                           value read = read_value(context, from);
                           return std::expected<late_move_shape, ast_node_type>(std::in_place,
                                                                                read.at("raw").as_integer()
                                                                               );
                       });

    // `late_move_shape` has no serializer, so this cannot go through `compose_checked`.
    formats fmts = formats::compose({ formats_builder()
                                          .type<circle>()
                                              .member("radius", &circle::radius)
                                          .polymorphic_type<std::unique_ptr<shape>>("kind")
                                              .subtype<late_move_shape>("late")
                                              .subtype<circle>("circle")
                                          .register_container<std::vector<std::unique_ptr<shape>>>(),
                                      formats::defaults()
                                    });
    fmts.register_extractor(&extractor);

    {
        extraction_context cxt(fmts, std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open(R"([ { "kind": "late", "raw": 9 }, { "kind": "circle", "radius": "x" } ])");

        ensure(!cxt.extract<std::vector<std::unique_ptr<shape>>>(rdr).has_value());
        ensure_eq(2U, cxt.problems().size());
        ensure(mentions(cxt.problems().at(0), "move refuses"));
        ensure_eq(path::create("[1].radius"), cxt.problems().at(1).path());
    }

    {
        extraction_context cxt(fmts, std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = open(R"([ { "kind": "late", "raw": 1 }, { "kind": "late", "raw": 9 } ])");

        ensure(!cxt.extract<std::vector<std::unique_ptr<shape>>>(rdr).has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure(mentions(cxt.problems().at(0), "move refuses"));
    }
}

TEST(composite_polymorphic_repeated_discriminator_agrees_with_the_subtype)
{
    // The subtype chosen and the subtype built read the same repeated key, so they must agree on which of its values
    // counts -- or a subtype comes back holding a discriminator for some other subtype, and `check` refuses to write
    // it out again. `ignore` keeps the first and `replace` the last, whether the discriminator reads one member or the
    // whole value.
    formats fmts = formats_builder()
                       .type<plain_tag>()
                           .member("kind", &plain_tag::kind)
                       .type<other_tag>()
                           .member("kind", &other_tag::kind)
                       .polymorphic_type<std::unique_ptr<shape>>("kind")
                           .subtype<plain_tag>("plain", keyed_subtype_action::check)
                           .subtype<other_tag>("other", keyed_subtype_action::check)
                       .polymorphic_type<std::shared_ptr<shape>>()
                           .subtype<plain_tag>([] (const value& from) { return from.at("kind") == value("plain"); })
                           .subtype<other_tag>([] (const value& from) { return from.at("kind") == value("other"); })
                   .compose_checked(formats::defaults());

    const std::string_view text = R"({ "kind": "plain", "kind": "other" })";

    using action = extract_options::duplicate_key_action;
    for (auto [on_duplicate, expected] : { std::pair(action::ignore, std::string("plain")),
                                           std::pair(action::replace, std::string("other")),
                                         }
        )
    {
        auto options = extract_options::create_default().on_duplicate_key(on_duplicate);

        {
            extraction_context cxt(fmts, std::nullopt, jsonv::path(), nullptr, options);
            auto               rdr = open(text);

            auto out = cxt.extract<std::unique_ptr<shape>>(rdr);
            ensure_eq(std::string_view(expected), name_of(out));
            ensure_eq(value(expected), to_json(*out, fmts).at("kind"));
        }

        {
            extraction_context cxt(fmts, std::nullopt, jsonv::path(), nullptr, options);
            auto               rdr = open(text);

            auto out = cxt.extract<std::shared_ptr<shape>>(rdr);
            ensure(out.has_value());
            ensure_eq(std::string_view(expected), (*out)->name());
        }
    }
}

TEST(composite_polymorphic_repeated_key_inside_a_discriminator_agrees_with_the_subtype)
{
    // The repeat is inside the discriminator's value, so it is settled by whatever materialises that value -- the
    // peek for choosing, the built-in `value` extractor for building. The two have to settle it alike, or the subtype
    // built holds a discriminator for the other one and `check` refuses to write it out again.
    formats fmts = formats_builder()
                       .type<first_nested>()
                           .member("kind", &first_nested::kind)
                       .type<second_nested>()
                           .member("kind", &second_nested::kind)
                       .polymorphic_type<std::unique_ptr<shape>>("kind")
                           .subtype<first_nested>(parse(R"({ "n": 1 })"), keyed_subtype_action::check)
                           .subtype<second_nested>(parse(R"({ "n": 2 })"), keyed_subtype_action::check)
                   .compose_checked(formats::defaults());

    const std::string_view text = R"({ "kind": { "n": 1, "n": 2 } })";

    using action = extract_options::duplicate_key_action;
    for (auto [on_duplicate, expected, kind] : { std::tuple(action::ignore,  "first_nested",  R"({ "n": 1 })"),
                                                 std::tuple(action::replace, "second_nested", R"({ "n": 2 })"),
                                               }
        )
    {
        auto options = extract_options::create_default().on_duplicate_key(on_duplicate);

        extraction_context cxt(fmts, std::nullopt, jsonv::path(), nullptr, options);
        auto               rdr = open(text);

        auto out = cxt.extract<std::unique_ptr<shape>>(rdr);
        ensure_eq(std::string_view(expected), name_of(out));
        ensure_eq(parse(kind), to_json(*out, fmts).at("kind"));
    }

    auto options = extract_options::create_default().on_duplicate_key(action::exception);

    extraction_context cxt(fmts, std::nullopt, jsonv::path(), nullptr, options);
    auto               rdr = open(text);

    ensure(!cxt.extract<std::unique_ptr<shape>>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string(R"(Duplicate key in object: "n")"), cxt.problems().at(0).message());
    ensure(rdr.current_type() == ast_node_type::object_begin);
}

TEST(composite_polymorphic_refused_discriminator_is_placed_where_the_context_says)
{
    // A repeated discriminator is refused while choosing, which reads ahead with a second cursor -- but where the
    // refusal belongs is still the extraction's to say, as it would be for the subtype reading the same key.
    formats fmts = formats_builder()
                       .type<first_nested>()
                           .member("kind", &first_nested::kind)
                       .polymorphic_type<std::unique_ptr<shape>>("kind")
                           .subtype<first_nested>(parse(R"({ "n": 1 })"))
                   .compose_checked(formats::defaults());

    const auto options = extract_options::create_default()
                             .on_duplicate_key(extract_options::duplicate_key_action::exception);

    auto placed = [&] (std::string_view text, jsonv::path base, std::optional<std::string_view> scope)
                  {
                      extraction_context                            cxt(fmts, std::nullopt, std::move(base), nullptr,
                                                                        options
                                                                       );
                      std::optional<extraction_context::path_scope> named;
                      if (scope)
                          named.emplace(cxt, *scope);

                      auto rdr = open(text);
                      ensure(!cxt.extract<std::unique_ptr<shape>>(rdr).has_value());
                      ensure_eq(1U, cxt.problems().size());
                      ensure(mentions(cxt.problems().at(0), "Duplicate key in object"));
                      return cxt.problems().at(0).path();
                  };

    const std::string_view repeated = R"({ "kind": { "n": 1 }, "kind": { "n": 1 } })";
    const std::string_view inside   = R"({ "kind": { "n": 1, "n": 1 } })";

    ensure_eq(path::create(".payload.kind"),   placed(repeated, path::create(".payload"), std::nullopt));
    ensure_eq(path::create(".payload.kind.n"), placed(inside,   path::create(".payload"), std::nullopt));
    ensure_eq(path::create(".renamed.kind"),   placed(repeated, jsonv::path(),            "renamed"));
    ensure_eq(path::create(".kind"),           placed(repeated, jsonv::path(),            std::nullopt));
}

}
