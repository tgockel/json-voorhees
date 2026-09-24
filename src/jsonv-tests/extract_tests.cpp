/// \file
/// Tests for the reader-based \c extractor interface.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "allocation_counter.hpp"
#include "test.hpp"

#include <jsonv/ast.hpp>
#include <jsonv/detail/reserve.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization_builder.hpp>
#include <jsonv/serialization/adapter_for.hpp>
#include <jsonv/serialization/function_extractor.hpp>
#include <jsonv/value.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>

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

/// An extractor written directly against the `void*` interface, which is the thing most implementations should not
/// have to do -- so it is the thing worth having a test for.
class bounded_int_extractor final :
        public extractor
{
public:
    const std::type_info& get_type() const noexcept override
    {
        return typeid(std::int64_t);
    }

    std::expected<void, ast_node_type>
    extract(extraction_context& context, reader& from, void* into) const override
    {
        auto node = context.current_as<ast_node::integer>(from);
        if (!node)
            return std::unexpected(node.error());

        auto found = node->value();
        (void) from.next_token();

        if (found < 500 || found > 2500)
            return context.problem(context.path(), "Expected a value between 500 and 2500");

        new(into) std::int64_t(found);
        return {};
    }
};

formats bounded_formats()
{
    static bounded_int_extractor instance;

    formats out = formats::compose({ formats::defaults() });
    out.register_extractor(&instance, duplicate_type_action::replace);
    return out;
}

/// Three required integer members, so a document which puts something else in one is one bad member and nothing
/// else -- which is what makes counting the problems a `collect_all` run reports meaningful.
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

formats triple_formats()
{
    static formats instance =
        formats_builder()
            .type<triple>()
                .member("a", &triple::a)
                .member("b", &triple::b)
                .member("c", &triple::c)
            .register_container<std::vector<triple>>()
            .register_container<std::vector<std::int64_t>>()
        .compose_checked(formats::defaults())
        ;

    return instance;
}

/// A `triple` whose `a` falls back to a factory which fails -- user code running outside the member's own extraction,
/// so it fails with whatever it threw rather than with an `extraction_error`.
formats seeded_triple_formats()
{
    static formats instance =
        formats_builder()
            .type<triple>()
                .member("a", &triple::a)
                    .default_value([] (extraction_context&) -> std::int64_t
                                   {
                                       throw std::out_of_range("no seed to derive `a` from");
                                   }
                                  )
                .member("b", &triple::b)
                .member("c", &triple::c)
        .compose_checked(formats::defaults())
        ;

    return instance;
}

/// A type whose adapter reports every problem it found in one go, the way a validator checking a whole record at
/// once does. `max_failures` is a threshold extraction stops at, not a truncation applied to what a single failure
/// reports, so a batch like this arrives whole.
struct batched { };

class batch_problem_adapter final :
        public adapter_for<batched>
{
protected:
    std::expected<batched, ast_node_type> create(extraction_context& context, reader& from) const override
    {
        (void) from.next_value();

        (void) context.problem(context.path(), "first");
        (void) context.problem(context.path(), "second");
        return context.problem(context.path(), "third");
    }

    value to_json(const serialization_context&, const batched&) const override
    {
        return value();
    }
};

formats batched_formats()
{
    static batch_problem_adapter instance;

    formats out = formats::compose({ formats::defaults() });
    out.register_adapter(&instance, duplicate_type_action::replace);
    return out;
}

/// A `std::int64_t` reached through the older `value`-based interface, reporting a mismatch the way every built-in
/// did before they read the reader directly: by letting `value::as_integer` throw. The bridge is still how
/// `container_adapter`, the DSL and the polymorphic adapters reach a value, so what it does with a failure is still
/// worth pinning.
struct bridged_int
{
    std::int64_t value;
};

class bridged_int_adapter final :
        public value_adapter_for<bridged_int>
{
protected:
    bridged_int create(extraction_context&, const value& from) const override
    {
        return bridged_int{ from.as_integer() };
    }

    value to_json(const serialization_context&, const bridged_int& from) const override
    {
        return value(from.value);
    }
};

/// A type whose member is a view of whatever it was extracted from, which is only valid while that source is. Used
/// from both sides of the materialisation question, so it lives out here rather than inside either test.
struct view_holder
{
    std::string_view s;
};

/// Three members reached through the older `value`-based interface. Materialising the object is what walks the cursor
/// past it and looking each member up by name is what needs the whole tree, so this is the shape the DSL had before it
/// read the reader -- kept here because the bridge is still how `polymorphic_adapter` and `enum_adapter` reach a value,
/// and what it does with a failure is still worth pinning.
struct bridged_triple
{
    std::int64_t a;
    std::int64_t b;
    std::int64_t c;
};

class bridged_triple_adapter final :
        public value_adapter_for<bridged_triple>
{
protected:
    bridged_triple create(extraction_context& context, const value& from) const override
    {
        bridged_triple out;
        out.a = context.extract<std::int64_t>(from.at("a"));
        out.b = context.extract<std::int64_t>(from.at("b"));
        out.c = context.extract<std::int64_t>(from.at("c"));
        return out;
    }

    value to_json(const serialization_context&, const bridged_triple&) const override
    {
        return object();
    }
};

formats bridged_formats()
{
    static bridged_int_adapter    int_instance;
    static bridged_triple_adapter triple_instance;

    formats out = formats::compose({ formats::defaults() });
    out.register_adapter(&int_instance);
    out.register_adapter(&triple_instance);
    return out;
}

extract_options collecting(extract_options::size_type max_failures = 10U)
{
    return extract_options::create_default()
                .failure_mode(extract_options::on_error::collect_all)
                .max_failures(max_failures);
}

/// The paths of every problem an `extraction_error` carries, in document order and joined, so a mismatch prints
/// the whole list rather than the first difference.
std::string problem_paths(const extraction_error& err)
{
    std::ostringstream os;
    bool               first = true;
    for (const auto& problem : err.problems())
    {
        if (!std::exchange(first, false))
            os << " ";
        os << problem.path();
    }

    return std::move(os).str();
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

/// Push \a depth nested \c path_scope guards -- alternating the two forms which own nothing -- and run \a at_bottom
/// inside the innermost one.
template <typename FAtBottom>
void with_nested_path_scopes(extraction_context& cxt, std::size_t depth, const FAtBottom& at_bottom)
{
    if (depth == 0U)
    {
        at_bottom();
    }
    else if (depth % 2U == 0U)
    {
        extraction_context::path_scope scope(cxt, depth);
        with_nested_path_scopes(cxt, depth - 1U, at_bottom);
    }
    else
    {
        // A literal, so it outlives the scope viewing it -- which is the requirement the `string_view` form puts on
        // its caller in exchange for not copying the key.
        extraction_context::path_scope scope(cxt, std::string_view("k"));
        with_nested_path_scopes(cxt, depth - 1U, at_bottom);
    }
}

/// Member names past every small-string buffer, so a design which copies the key in order to name it pays a visible
/// string allocation rather than hiding inside SSO.
const std::string long_a = "member_alpha_with_a_deliberately_long_name";
const std::string long_b = "member_bravo_with_a_deliberately_long_name";
const std::string long_c = "member_carol_with_a_deliberately_long_name";

/// Three members reached through the DSL, which names each one as it descends into it.
struct named_triple
{
    std::int64_t a;
    std::int64_t b;
    std::int64_t c;
};

/// The same three members reached by an adapter which names nothing, so that comparing the two says what naming
/// costs and nothing else. Everything else either one does -- the enclosing context, the key walk, the lookup, the
/// container it lands in -- is identical and cancels.
struct unnamed_triple
{
    std::int64_t a;
    std::int64_t b;
    std::int64_t c;
};

/// The DSL's key loop with the `path_scope` taken out and not one other thing changed. It was written against the
/// `value` interface while the DSL was, and had to follow it onto the reader: an adapter which materialises has a
/// wholly different cost, so comparing one against the other would measure the materialisation and not the naming.
class unnamed_triple_adapter final :
        public adapter_for<unnamed_triple>
{
protected:
    JSONV_NODISCARD
    virtual std::expected<unnamed_triple, ast_node_type> create(extraction_context& context,
                                                                reader&             from
                                                               ) const override
    {
        auto opened = context.current_as<ast_node::object_begin>(from);
        if (!opened)
            return std::unexpected(opened.error());

        unnamed_triple out;

        (void) from.next_token();
        while (from.good() && from.current().type() != ast_node_type::object_end)
        {
            auto key = from.current().as<ast_node::key_canonical>().value();
            if (!from.next_token())
                break;

            auto member = context.extract<std::int64_t>(from);
            if (!member)
                return std::unexpected(member.error());

            if (key == long_a)
                out.a = *member;
            else if (key == long_b)
                out.b = *member;
            else
                out.c = *member;
        }

        (void) from.next_token();
        return out;
    }

    JSONV_NODISCARD
    virtual value to_json(const serialization_context&, const unnamed_triple&) const override
    {
        return value();
    }
};

/// A `std::vector<std::int64_t>` filled by an adapter which names nothing -- `container_adapter`'s loop with the
/// `path_scope` taken out and not one other thing changed.
struct unnamed_vector
{
    std::vector<std::int64_t> values;

    JSONV_NODISCARD
    std::size_t size() const
    {
        return values.size();
    }
};

class unnamed_vector_adapter final :
        public adapter_for<unnamed_vector>
{
protected:
    JSONV_NODISCARD
    virtual std::expected<unnamed_vector, ast_node_type> create(extraction_context& context,
                                                                reader&             from
                                                               ) const override
    {
        using std::end;

        auto opened = context.current_as<ast_node::array_begin>(from);
        if (!opened)
            return std::unexpected(opened.error());

        unnamed_vector out;
        jsonv::detail::reserve_if_possible(out.values, opened->element_count());

        (void) from.next_token();
        while (from.good())
        {
            if (from.current().type() == ast_node_type::array_end)
            {
                (void) from.next_token();
                return out;
            }

            auto element = context.extract<std::int64_t>(from);
            if (!element)
                return std::unexpected(element.error());

            out.values.insert(end(out.values), *std::move(element));
        }

        return context.problem(context.problem_path(from), "Unterminated array");
    }

    JSONV_NODISCARD
    virtual value to_json(const serialization_context&, const unnamed_vector&) const override
    {
        return array();
    }
};

/// The named and unnamed halves of both comparisons, in one `formats` so a single context reaches all of them.
const formats& path_cost_formats()
{
    static const formats instance =
        [] ()
        {
            static unnamed_triple_adapter                         unnamed_triple_instance;
            static unnamed_vector_adapter                         unnamed_vector_instance;
            static container_adapter<std::vector<unnamed_triple>> unnamed_triple_vector_instance;

            formats named = formats_builder()
                                .type<named_triple>()
                                    .member(long_a, &named_triple::a)
                                    .member(long_b, &named_triple::b)
                                    .member(long_c, &named_triple::c)
                                .register_container<std::vector<named_triple>>()
                                .register_container<std::vector<std::int64_t>>()
                            .compose_checked(formats::defaults());

            formats out = formats::compose({ named });
            out.register_adapter(&unnamed_triple_instance,        duplicate_type_action::replace);
            out.register_adapter(&unnamed_vector_instance,        duplicate_type_action::replace);
            out.register_adapter(&unnamed_triple_vector_instance, duplicate_type_action::replace);
            return out;
        }();

    return instance;
}

/// An array of \a count integers.
value integers(std::size_t count)
{
    value out = array();
    for (std::size_t idx = 0U; idx < count; ++idx)
        out.push_back(std::int64_t(idx));

    return out;
}

/// An array of \a count objects carrying the three long member names.
value named_triples(std::size_t count)
{
    value out = array();
    for (std::size_t idx = 0U; idx < count; ++idx)
        out.push_back(object({ { long_a, std::int64_t(idx) }, { long_b, 2 }, { long_c, 3 } }));

    return out;
}

/// The allocations a fresh extraction of \c T from \a source performs. The result's size is checked too, so an
/// extraction which quietly produced nothing cannot pass for a cheap one.
template <typename T>
std::size_t extraction_cost(const value& source)
{
    extraction_context cxt(path_cost_formats());

    allocation_counter allocations;
    const auto         out  = cxt.extract<T>(source);
    const std::size_t  cost = allocations.count();

    ensure_eq(source.size(), out.size());
    return cost;
}

#endif

}

TEST(extract_reader_extractor_success)
{
    extraction_context cxt(bounded_formats());
    auto               rdr = open("1234");

    auto result = cxt.extract<std::int64_t>(rdr);
    ensure(result.has_value());
    ensure_eq(1234, *result);
    ensure(cxt.problems().empty());
}

TEST(extract_reader_extractor_problem)
{
    extraction_context cxt(bounded_formats());
    auto               rdr = open("7");

    auto result = cxt.extract<std::int64_t>(rdr);
    ensure(!result.has_value());
    ensure(result.error() == ast_node_type::error);
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Expected a value between 500 and 2500"), cxt.problems().at(0).message());
}

TEST(extract_reader_extractor_type_mismatch_carries_the_type_found)
{
    extraction_context cxt(bounded_formats());
    auto               rdr = open(R"("not a number")");

    auto result = cxt.extract<std::int64_t>(rdr);
    ensure(!result.has_value());

    // A mismatch reports what was actually there rather than the `error` sentinel.
    ensure(result.error() == ast_node_type::string_canonical);
    ensure_eq(1U, cxt.problems().size());
}

TEST(extract_context_expect_single_type)
{
    extraction_context cxt;
    auto               rdr = open("true");

    auto matched = cxt.expect(rdr, ast_node_type::integer);
    ensure(!matched.has_value());
    ensure(matched.error() == ast_node_type::literal_true);
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Read node of type true when expecting integer"), cxt.problems().at(0).message());

    ensure(cxt.expect(rdr, ast_node_type::literal_true).has_value());
    ensure_eq(1U, cxt.problems().size());
}

TEST(extract_context_expect_type_list)
{
    extraction_context cxt;
    auto               rdr = open("true");

    auto matched = cxt.expect(rdr, { ast_node_type::integer, ast_node_type::decimal });
    ensure(!matched.has_value());
    ensure(matched.error() == ast_node_type::literal_true);
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Read node of type true when expecting one of integer, decimal"),
              cxt.problems().at(0).message()
             );

    ensure(cxt.expect(rdr, { ast_node_type::integer, ast_node_type::literal_true }).has_value());
    ensure_eq(1U, cxt.problems().size());
}

TEST(extract_context_expect_reports_the_readers_path)
{
    extraction_context cxt;
    auto               rdr = open(R"({ "a": [ 1, "two" ] })");

    // Walk to the "two" so the reader has a non-trivial path to report the problem at.
    while (rdr.good() && rdr.current().type() != ast_node_type::string_canonical)
        (void) rdr.next_token();

    ensure(!cxt.expect(rdr, ast_node_type::integer).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[1]"), cxt.problems().at(0).path());
}

TEST(extract_context_current_as)
{
    extraction_context cxt;
    auto               rdr = open("42");

    auto node = cxt.current_as<ast_node::integer>(rdr);
    ensure(node.has_value());
    ensure_eq(42, node->value());
    ensure(cxt.problems().empty());

    auto mismatch = cxt.current_as<ast_node::decimal>(rdr);
    ensure(!mismatch.has_value());
    ensure(mismatch.error() == ast_node_type::integer);
    ensure_eq(1U, cxt.problems().size());
}

TEST(extract_path_scope_restores_on_exit)
{
    extraction_context cxt;
    ensure_eq(path(), cxt.path());

    {
        extraction_context::path_scope outer(cxt, std::string_view("a"));
        ensure_eq(path::create(".a"), cxt.path());

        {
            extraction_context::path_scope inner(cxt, std::size_t(3));
            ensure_eq(path::create(".a[3]"), cxt.path());
        }

        ensure_eq(path::create(".a"), cxt.path());
    }

    ensure_eq(path(), cxt.path());
}

TEST(extract_path_scope_restores_when_unwound_by_an_exception)
{
    extraction_context cxt;

    try
    {
        extraction_context::path_scope outer(cxt, std::string_view("a"));
        extraction_context::path_scope inner(cxt, path_element(std::string("owned")));
        ensure_eq(path::create(".a.owned"), cxt.path());
        throw std::runtime_error("unwind");
    }
    catch (const std::runtime_error&)
    { }

    ensure_eq(path(), cxt.path());

    // An `_innermost` left naming a destroyed frame would report whatever that stack now holds rather than just
    // `.after`. ASan is where that shows up, as a stack-use-after-scope rather than a quietly wrong answer.
    extraction_context::path_scope after(cxt, std::string_view("after"));
    ensure_eq(path::create(".after"), cxt.path());
}

TEST(extract_path_scope_takes_precedence_over_the_readers_path)
{
    extraction_context cxt;
    auto               rdr = open(R"({ "a": "wrong" })");

    while (rdr.good() && rdr.current().type() != ast_node_type::string_canonical)
        (void) rdr.next_token();

    // The reader would say ".a"; the scope says where the *extractor* thinks it is, and wins.
    extraction_context::path_scope scope(cxt, std::string_view("renamed"));
    ensure(!cxt.expect(rdr, ast_node_type::integer).has_value());
    ensure_eq(path::create(".renamed"), cxt.problems().at(0).path());
}

TEST(extract_member_scope_ends_before_the_setter)
{
    // The member's scope names where the *extraction* is, not where the assignment is. A setter supplied through
    // the `member(name, access, mutate)` overload is arbitrary user code, and once the member's value exists the
    // extractor is no longer at that key -- anything the setter goes on to extract sits where the document puts
    // it rather than underneath the member whose setter happens to be running. `pre_extract` is handed the
    // context precisely so user code can reach it, which is what lets this observe the boundary at all.
    struct probe
    {
        std::int64_t a;
    };

    extraction_context* captured = nullptr;
    jsonv::path         seen_by_setter = path::create(".neverran");

    const formats fmts =
        formats_builder()
            .type<probe>()
                .pre_extract([&captured] (extraction_context& cxt) { captured = &cxt; })
                .member<std::int64_t>("a",
                                      [] (const probe& x) -> const std::int64_t& { return x.a; },
                                      [&] (probe& x, std::int64_t&& value)
                                      {
                                          seen_by_setter = captured->path();
                                          x.a            = value;
                                      }
                                     )
        .compose_checked(formats::defaults())
        ;

    const auto out = extract<probe>(parse(R"({ "a": 7 })"), fmts);

    ensure_eq(7, out.a);
    ensure_eq(path(), seen_by_setter);
}

TEST(extract_allocation_counting_is_available)
{
    // The cost assertions below are compiled out where the counter is, which is right under a sanitizer build and
    // would be silent breakage anywhere else -- the tests would simply stop existing and the run would stay green.
    // CMake knows whether sanitizers were asked for, so it is the one thing in the build that can tell the counter
    // it is wrong; this asks it, in both directions, and deliberately lives outside the guard it is checking.
#if JSONV_TEST_REQUIRE_ALLOCATION_COUNTING
    ensure(JSONV_TEST_COUNTS_ALLOCATIONS != 0);
#else
    ensure(JSONV_TEST_COUNTS_ALLOCATIONS == 0);
#endif
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

TEST(extract_path_scope_push_and_pop_allocate_nothing)
{
    // The path is read only when something goes wrong, so maintaining it has to be free when nothing does: a scope
    // is a stack object linked into a chain, and descending into an element costs two stores rather than a copy of
    // everything above it. A regression here would otherwise surface only as a slightly worse benchmark number.
    extraction_context cxt;

    for (std::size_t depth : { std::size_t(1U), std::size_t(64U), std::size_t(1024U) })
    {
        allocation_counter allocations;
        with_nested_path_scopes(cxt, depth, [] { });
        const std::size_t cost = allocations.count();

        ensure_eq(0U, cost);
        ensure(cxt.path().empty());
    }

    // The chain still says where it is. Asked outside a counted region, because building a `jsonv::path` is exactly
    // the work this design defers to the moment something actually fails.
    jsonv::path deepest;
    with_nested_path_scopes(cxt, 3U, [&] { deepest = cxt.path(); });
    ensure_eq(path::create(".k[2].k"), deepest);
}

TEST(extract_container_element_naming_costs_nothing)
{
    // The same array through two adapters which differ only in whether the element is named. Everything else -- the
    // context, the reader the bridge builds per element, the extraction itself, the vector it lands in and how that
    // vector grows -- is identical and cancels, so the two counts are equal exactly when naming is free. Before this
    // change `container_adapter` reached the scope through `extract_sub`, which builds a `jsonv::path` from its
    // argument, and the named arm exceeded the unnamed one by one allocation per element.
    const value source = integers(256U);

    // Discarded: the first extraction of a type pays for whatever the formats cache on first use, which is not
    // per-element and not what is being compared.
    (void) extraction_cost<std::vector<std::int64_t>>(source);
    (void) extraction_cost<unnamed_vector>(source);

    const std::size_t named   = extraction_cost<std::vector<std::int64_t>>(source);
    const std::size_t unnamed = extraction_cost<unnamed_vector>(source);

    ensure_eq(unnamed, named);
}

TEST(extract_member_naming_costs_nothing)
{
    // The container half of this is the same on both sides, so what is left is the DSL's member loop against an
    // adapter reading the same three members without naming them. The names are long on purpose: the old code
    // copied the key into a `path_element`, which a short name hides inside the small-string buffer.
    const value source = named_triples(64U);

    (void) extraction_cost<std::vector<named_triple>>(source);
    (void) extraction_cost<std::vector<unnamed_triple>>(source);

    const std::size_t named   = extraction_cost<std::vector<named_triple>>(source);
    const std::size_t unnamed = extraction_cost<std::vector<unnamed_triple>>(source);

    ensure_eq(unnamed, named);
}

TEST(extract_path_tracking_does_not_grow_with_the_base_path)
{
    // A context extracting a fragment of some larger document carries the path to that fragment. `_base_path` is
    // read only by `path()`, which a successful extraction never calls, so how deep the fragment sits changes
    // nothing. This is the guard against the design this one replaced, where a scope saved and restored a copy of
    // the whole current path and every push would have copied those five elements and their five keys.
    const value source = named_triples(16U);

    auto cost_with = [&source] (jsonv::path base) -> std::size_t
                     {
                         extraction_context cxt(path_cost_formats(), std::nullopt, std::move(base));

                         allocation_counter allocations;
                         const auto         out  = cxt.extract<std::vector<named_triple>>(source);
                         const std::size_t  cost = allocations.count();

                         ensure_eq(source.size(), out.size());
                         return cost;
                     };

    (void) cost_with(jsonv::path());

    ensure_eq(cost_with(jsonv::path()), cost_with(path::create(".a.b.c.d.e")));
}

#endif

TEST(extract_read_value_scalars)
{
    ensure_eq(value(true),      [] { auto r = open("true");     return read_value(r); }());
    ensure_eq(value(false),     [] { auto r = open("false");    return read_value(r); }());
    ensure_eq(value(),          [] { auto r = open("null");     return read_value(r); }());
    ensure_eq(value(42),        [] { auto r = open("42");       return read_value(r); }());
    ensure_eq(value(4.5),       [] { auto r = open("4.5");      return read_value(r); }());
    ensure_eq(value("thing"),   [] { auto r = open(R"("thing")"); return read_value(r); }());
    ensure_eq(value("a\"b"),    [] { auto r = open(R"("a\"b")"); return read_value(r); }());
}

TEST(extract_read_value_structures)
{
    auto empty_object = open("{}");
    ensure_eq(object(), read_value(empty_object));

    auto empty_array = open("[]");
    ensure_eq(array(), read_value(empty_array));

    auto nested = open(R"({ "a": [ 1, { "b": null } ], "c": "d" })");
    ensure_eq(parse(R"({ "a": [ 1, { "b": null } ], "c": "d" })"), read_value(nested));
}

TEST(extract_read_value_works_on_a_fresh_reader)
{
    // No explicit step off document_start -- read_value does it.
    reader rdr(R"([ 1, 2 ])");
    ensure_eq(parse("[1, 2]"), read_value(rdr));
}

TEST(extract_read_value_lands_one_past_the_subtree)
{
    // Every entry here is an array whose first element read_value consumes; what follows is what the cursor must be
    // sitting on afterwards.
    auto check = [](std::string_view source)
                 {
                     auto rdr = open(source);
                     ensure(rdr.current().type() == ast_node_type::array_begin);
                     (void) rdr.next_token();

                     (void) read_value(rdr);

                     ensure(rdr.good());
                     ensure(rdr.current().type() == ast_node_type::integer);
                     ensure_eq(99, rdr.current().as<ast_node::integer>().value());
                 };

    check("[ 1, 99 ]");
    check(R"([ "s", 99 ])");
    check("[ null, 99 ]");
    check("[ [], 99 ]");
    check("[ {}, 99 ]");
    check("[ [ 1, [ 2 ] ], 99 ]");
    check(R"([ { "a": { "b": [ 1, 2 ] } }, 99 ])");
}

TEST(extract_read_value_lands_one_past_an_object_member)
{
    auto rdr = open(R"({ "a": [ 1, 2 ], "b": 7 })");
    (void) rdr.next_token();   // onto "a"
    (void) rdr.next_token();   // onto [

    ensure_eq(parse("[1, 2]"), read_value(rdr));

    // Landing on the `}` instead would silently drop every member after the one that was read.
    ensure(rdr.good());
    ensure(rdr.current().type() == ast_node_type::key_canonical);
    ensure_eq(std::string("b"), std::string(rdr.current().as<ast_node::key_canonical>().value()));
}

TEST(extract_read_value_lands_one_past_on_a_value_sourced_reader)
{
    // `reader::impl_value` walks a tree and `reader::impl_parse_index` walks a tape; they implement stepping
    // separately, and the bridge runs on the former. Both have to land in the same place.
    auto check = [](const value& source)
                 {
                     reader rdr = reader::from_value(source);
                     (void) rdr.next_token();
                     ensure(rdr.current().type() == ast_node_type::array_begin);
                     (void) rdr.next_token();

                     (void) read_value(rdr);

                     ensure(rdr.good());
                     ensure(rdr.current().type() == ast_node_type::integer);
                     ensure_eq(99, rdr.current().as<ast_node::integer>().value());
                 };

    check(parse("[ 1, 99 ]"));
    check(parse(R"([ "s", 99 ])"));
    check(parse("[ [], 99 ]"));
    check(parse("[ {}, 99 ]"));
    check(parse(R"([ { "a": { "b": [ 1, 2 ] } }, 99 ])"));
}

TEST(extract_read_value_rejects_a_non_value)
{
    auto rdr = open(R"({ "a": 1 })");
    (void) rdr.next_token();   // onto the key, which is not a value
    ensure_throws(extraction_error, read_value(rdr));
}

namespace
{

struct nested_failure
{
    std::string  first;
    std::int64_t second;
};

/// Reports a problem *after* a successful sub-extraction, so ASan can say whether the first one leaked.
class nested_failure_adapter final :
        public adapter_for<nested_failure>
{
protected:
    std::expected<nested_failure, ast_node_type>
    create(extraction_context& context, reader& from) const override
    {
        if (auto opened = context.expect(from, ast_node_type::array_begin); !opened)
            return std::unexpected(opened.error());

        (void) from.next_token();

        nested_failure out;
        if (auto first = context.extract<std::string>(from))
            out.first = std::move(*first);
        else
            return std::unexpected(first.error());

        return context.problem(context.path(), "Second element is unacceptable");
    }

    value to_json(const serialization_context&, const nested_failure&) const override
    {
        return array();
    }
};

}

TEST(extract_no_leak_when_a_later_step_fails)
{
    static nested_failure_adapter instance;

    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_adapter(&instance);

    extraction_context cxt(fmts);
    auto               rdr = open(R"([ "a string long enough to need the heap rather than the small-string buffer", 2 ])");

    auto result = cxt.extract<nested_failure>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
}

TEST(extract_problems_survive_the_value_bridge)
{
    // `unassociated` has no extractor, so the failure happens below a value-based adapter and has to come back up
    // through the catch boundary in extraction_context::extract with its path and cause intact.
    struct unassociated { };

    extraction_context cxt(formats::defaults());
    value              val = parse(R"({ "o": { "i": 5 } })");

    try
    {
        (void) cxt.extract_sub<unassociated>(val, "o");
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure_eq(path::create(".o"), err.problems().at(0).path());
        ensure(err.problems().at(0).nested_ptr());
        ensure_throws(no_extractor, (std::rethrow_exception(err.problems().at(0).nested_ptr()), 0));
    }

    // The problems went into the exception rather than being left behind to be reported a second time.
    ensure(cxt.problems().empty());
}

TEST(extract_borrowed_string_view_points_at_the_source)
{
    // The built-in `std::string_view` adapter returns a view of what it was handed. Running extraction through a
    // reader must not turn that into a view of a temporary the bridge materialised and then destroyed.
    value source = std::string("long enough to be on the heap rather than in the small-string buffer");

    auto borrowed = extract<std::string_view>(source);
    ensure_eq(source.as_string_view(), borrowed);
    ensure(borrowed.data() == source.as_string_view().data());
}

TEST(extract_borrowed_string_view_survives_a_nested_adapter)
{
    // The same has to hold when the view comes back up through a bridged adapter rather than directly.
    formats fmts = formats::compose({ formats_builder()
                                          .register_container<std::vector<std::string_view>>(),
                                      formats::defaults()
                                    });

    value source = parse(R"([ "the first long string value here", "the second long string value here" ])");
    auto  views  = extract<std::vector<std::string_view>>(source, fmts);

    ensure_eq(2U, views.size());
    for (std::size_t idx = 0; idx < views.size(); ++idx)
        ensure(views.at(idx).data() == source.at(idx).as_string_view().data());
}

TEST(extract_value_backed_reader_lends_its_subtree)
{
    value  source = parse(R"({ "a": [ 1, 2 ], "b": 3 })");
    reader rdr    = reader::from_value(source);

    (void) rdr.next_token();   // onto {
    ensure(rdr.current_value() == &source);

    (void) rdr.next_token();   // onto the key "a" -- half a member, not a value
    ensure(rdr.current_value() == nullptr);

    (void) rdr.next_token();   // onto [
    ensure(rdr.current_value() == &source.at("a"));

    // A text-backed reader has no tree to lend.
    auto text = open(R"({ "a": 1 })");
    ensure(text.current_value() == nullptr);
}

TEST(extract_read_value_duplicate_keys_match_parse)
{
    // `parse` uses duplicate_key_action::replace, so the last one wins. Reading the same text through a reader has to
    // select the same member, or extracting from text and from the parsed tree disagree.
    for (const auto* source : { R"({"x":1,"x":2})",
                                R"({"x":1,"x":2,"x":3})",
                                R"({"a":0,"x":1,"x":2,"b":9})"
                              })
    {
        auto rdr = open(source);
        ensure_eq(parse(source), read_value(rdr));
    }
}

TEST(extract_bridged_failure_names_the_element_it_started_on)
{
    // The bridge consumes its subtree before the older body runs, so a conversion failure must still be reported at
    // the value the extraction started on rather than at the sibling the cursor has moved to.
    extraction_context cxt(formats::defaults());
    auto               rdr = open(R"([ "wrong", 99 ])");

    (void) rdr.next_token();   // onto "wrong", element [0]

    auto result = cxt.extract<std::int64_t>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
}

TEST(extract_error_message_separates_an_empty_path_from_the_message)
{
    try
    {
        auto rdr = open("}");
        (void) read_value(rdr);
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure(err.path().empty());
        ensure_eq(std::string("Extraction error: Unexpected token of type end of object while reading a value"),
                  std::string(err.what())
                 );
    }
}

TEST(extract_error_with_no_problems_synthesises_one)
{
    // `problems()` documents that the list is never empty, and unlike `path()` and `nested_ptr()` it has nothing
    // sensible to fall back on -- so the invariant is established when the error is built rather than guarded at
    // each accessor. A caller reading `problems()` and a caller reading `what()` should not disagree about whether
    // anything went wrong.
    extraction_error err{ extraction_error::problem_list() };

    ensure_eq(1U, err.problems().size());
    ensure(err.problems().at(0).path().empty());
    ensure(!err.problems().at(0).nested_ptr());
    ensure(!std::string(err.what()).empty());
}

TEST(extract_read_value_rejects_an_unmatched_close)
{
    // These reach `read_value` only from a tape which was never validated, and the diagnostic it builds asks the
    // reader where it is -- which used to walk off the end of the path builder's element stack.
    for (const auto* source : { "}", "]" })
    {
        auto rdr = open(source);
        ensure_throws(extraction_error, read_value(rdr));
    }
}

namespace
{

/// Overloaded on the constness of what it is handed, so which one runs is observable.
struct constness_probe
{
    std::string operator()(const value&) const { return "const"; }
    std::string operator()(value&) const       { return "mutable"; }
};

}

TEST(extract_legacy_dispatch_passes_a_const_value)
{
    // `invoke_extract` checks `invocable<F, const value&>` before choosing this shape, so it has to invoke with a
    // `const value&` too -- handing over the mutable local it materialised would run an overload the check never
    // tested, and can fail to compile when the two return different types.
    static auto instance = make_extractor(constness_probe());

    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_extractor(&instance, duplicate_type_action::replace);

    ensure_eq(std::string("const"), extract<std::string>(value("ignored"), fmts));
}

TEST(extract_string_view_from_text_points_at_the_source)
{
    // A canonical string token *is* the string, so a view of it is a view of the document the caller handed over --
    // no decoding, no copy, and valid for exactly as long as that source is.
    std::string        source = R"("long enough to be on the heap rather than in the small-string buffer")";
    extraction_context cxt(formats::defaults());
    reader             rdr(std::string_view{ source });
    (void) rdr.next_token();

    auto borrowed = cxt.extract<std::string_view>(rdr);
    ensure(borrowed.has_value());
    ensure_eq(std::string_view("long enough to be on the heap rather than in the small-string buffer"), *borrowed);
    ensure(borrowed->data() == source.data() + 1);
    ensure(cxt.problems().empty());
}

TEST(extract_string_view_from_an_escaped_string_is_refused)
{
    // An escaped string has no decoded form anywhere in the source to point at, so there is nothing to borrow. A
    // silent copy into storage which dies with the call is the thing worth refusing.
    extraction_context cxt(formats::defaults());
    auto               rdr = open(R"("a \u00e9 which the source spelt the long way")");

    auto result = cxt.extract<std::string_view>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure(cxt.problems().at(0).message().find("std::string_view") != std::string::npos);

    // `std::string` is the documented way to get the same text out, and decodes it.
    auto               other = open(R"("a \u00e9 which the source spelt the long way")");
    extraction_context decoding(formats::defaults());
    auto               decoded = decoding.extract<std::string>(other);
    ensure(decoded.has_value());
    // Explicit UTF-8 bytes rather than `\u00e9`: a universal character name in a narrow literal is encoded in the
    // compiler's execution character set, which is not UTF-8 everywhere, while the decoder always produces UTF-8.
    ensure_eq(std::string("a \xc3\xa9 which the source spelt the long way"), *decoded);   // U+00E9
}

TEST(extract_nested_string_view_from_text_points_at_the_source)
{
    // This was a refusal while `container_adapter` was on the bridge: the container materialised the whole array and
    // every element then borrowed from a temporary which died with the extraction. Walking the reader means there is
    // no temporary, so each element views the source document exactly as a lone `std::string_view` does -- and is
    // valid for exactly as long as that source is.
    formats fmts = formats::compose({ formats_builder()
                                          .register_container<std::vector<std::string_view>>(),
                                      formats::defaults()
                                    });

    std::string        source = R"([ "the first long string value here", "the second long string value here" ])";
    extraction_context cxt(fmts);
    reader             rdr(std::string_view{ source });
    (void) rdr.next_token();

    auto views = cxt.extract<std::vector<std::string_view>>(rdr);
    ensure(views.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(2U, views->size());
    ensure_eq(std::string_view("the first long string value here"),  views->at(0));
    ensure_eq(std::string_view("the second long string value here"), views->at(1));

    // Views of the document itself, not copies of it.
    for (const auto& view : *views)
        ensure(view.data() >= source.data() && view.data() < source.data() + source.size());
}

TEST(extract_string_view_under_a_bridged_composite_from_text_is_refused)
{
    // The coverage the tests above gave up. A composite which still materialises is exactly where a view of the
    // materialised tree would dangle, and `source_is_temporary` is what an extractor asks to notice it. Neither the
    // container nor the DSL creates that situation any anymore, so the composite is written against the `value`
    // interface directly -- which is what `polymorphic_adapter` and `enum_adapter` still do.
    class view_holder_adapter final :
            public value_adapter_for<view_holder>
    {
    protected:
        view_holder create(extraction_context& context, const value& from) const override
        {
            return view_holder{ context.extract<std::string_view>(from.at("s")) };
        }

        value to_json(const serialization_context&, const view_holder&) const override
        {
            return object();
        }
    };

    static view_holder_adapter instance;

    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_adapter(&instance, duplicate_type_action::replace);

    extraction_context cxt(fmts);
    auto               rdr = open(R"({ "s": "a long string value which would be left dangling" })");

    ensure(!cxt.extract<view_holder>(rdr).has_value());
    ensure(!cxt.problems().empty());
    ensure(cxt.problems().at(0).message().find("std::string_view") != std::string::npos);
}

TEST(extract_dsl_member_string_view_from_text_points_at_the_source)
{
    // The other half of the same change. A DSL-described type materialised its whole subtree, so a `std::string_view`
    // member had to be refused -- for a structural reason rather than a semantic one. With nothing materialised the
    // member views the document exactly as a lone `std::string_view` does.
    formats fmts = formats_builder()
                       .type<view_holder>()
                           .member("s", &view_holder::s)
                   .compose_checked(formats::defaults());

    std::string        source = R"({ "s": "a long string value which is not copied anywhere" })";
    extraction_context cxt(fmts);
    reader             rdr(std::string_view{ source });
    (void) rdr.next_token();

    auto held = cxt.extract<view_holder>(rdr);
    ensure(held.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(std::string_view("a long string value which is not copied anywhere"), held->s);

    // A view of the document itself, not a copy of it.
    ensure(held->s.data() >= source.data() && held->s.data() < source.data() + source.size());
}

TEST(extract_scope_free_loop_over_text_stays_linear)
{
    // Naming the position a failure started at by building a path before every extraction is quadratic here: a
    // text-backed `current_path` rescans from the start of the document, so element `i` costs `i`. Leaving the cursor
    // on the value until the extraction succeeds gets the same answer for nothing.
    //
    // The two are separated by about three orders of magnitude at this size -- roughly a second against a
    // millisecond -- so a generous absolute bound distinguishes them without being sensitive to the machine.
    const int elements = JSONV_DEBUG ? 4000 : 20000;

    std::string document = "[";
    for (int idx = 0; idx < elements; ++idx)
    {
        if (idx)
            document += ',';
        document += std::to_string(idx % 1000);
    }
    document += "]";

    auto started = std::chrono::steady_clock::now();

    extraction_context cxt(formats::defaults());
    jsonv::reader      rdr(document);
    (void) rdr.next_token();   // onto [
    (void) rdr.next_token();   // onto the first element

    std::int64_t total = 0;
    for (int idx = 0; idx < elements; ++idx)
    {
        auto element = cxt.extract<std::int64_t>(rdr);
        ensure(element.has_value());
        total += *element;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - started
                   ).count();

    ensure(cxt.problems().empty());
    ensure_gt(total, 0);
    ensure_lt(elapsed, 5000);
}

TEST(extract_bridged_structure_failure_names_the_enclosing_structure)
{
    // A structure has to be walked to be read, so unlike a scalar the cursor is past it before the older body runs.
    // The sibling it lands on is a perfectly valid value and naming it would send the reader of the error to the
    // wrong place; the structure around the failure is still true of it.
    {
        extraction_context cxt(bridged_formats());
        auto               rdr = open(R"({ "a": [ [], 99 ] })");

        while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
            (void) rdr.next_token();
        (void) rdr.next_token();   // onto the inner [], which is `.a[0]`

        auto result = cxt.extract<bridged_int>(rdr);
        ensure(!result.has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_ne(path::create(".a[1]"), cxt.problems().at(0).path());
        ensure_eq(path::create(".a"), cxt.problems().at(0).path());
    }

    // Landing on a closing token is the case where `current_path` already names the structure, so nothing is dropped.
    {
        extraction_context cxt(bridged_formats());
        auto               rdr = open(R"({ "a": [ [] ] })");

        while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
            (void) rdr.next_token();
        (void) rdr.next_token();   // onto the inner []

        ensure(!cxt.extract<bridged_int>(rdr).has_value());
        ensure_eq(path::create(".a"), cxt.problems().at(0).path());
    }
}

TEST(extract_native_structure_failure_names_the_value_it_was_on)
{
    // The other half of the case above: an extractor which reads the reader never moves off the value it rejected,
    // so the enclosing structure is not the best it can say. `.a[0]` is where the array actually is.
    extraction_context cxt(formats::defaults());
    auto               rdr = open(R"({ "a": [ [], 99 ] })");

    while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
        (void) rdr.next_token();
    (void) rdr.next_token();   // onto the inner [], which is `.a[0]`

    auto result = cxt.extract<std::int64_t>(rdr);
    ensure(!result.has_value());
    ensure(result.error() == ast_node_type::array_begin);
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[0]"), cxt.problems().at(0).path());
}

TEST(extract_stale_cursor_does_not_leak_into_a_nested_reader)
{
    // The bridge marks the reader it walked past, not a global flag: a nested extraction running on its own
    // value-backed reader must still get that reader's exact position. The DSL is the composite which still
    // materialises -- `optional_adapter` used to stand in here and now reads the reader, so it no longer would.
    extraction_context cxt(triple_formats());
    auto               rdr = open(R"([ { "a": "wrong", "b": 2, "c": 3 }, 99 ])");
    (void) rdr.next_token();   // onto the object

    // The object is materialised (cursor moves past it); the member extraction below runs on a fresh value reader.
    ensure(!cxt.extract<triple>(rdr).has_value());
    ensure(!cxt.problems().empty());
    ensure_eq(path::create(".a"), cxt.problems().at(0).path());
}

TEST(extract_malformed_key_does_not_escape_the_diagnostic)
{
    // Naming where a failure happened walks the object keys between here and the start of the document, and decoding
    // one of those can itself throw -- an unpaired surrogate does. That happens on the unwind, inside a `noexcept`
    // destructor, so getting it wrong terminates the process rather than reporting anything.
    extraction_context cxt(formats::defaults());
    auto               rdr = open(R"({ "a": [], "\uD800": 0 })");

    while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
        (void) rdr.next_token();

    auto result = cxt.extract<std::int64_t>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());

    // The message describes the extraction that failed, not the parse error hit while looking for a name for it.
    ensure(cxt.problems().at(0).message().find("surrogate") == std::string::npos);
}

TEST(extract_explicit_location_outranks_the_reader)
{
    // A scope, and a base path, say where the *extractor* is. Both are authoritative over anything worked out from
    // where the reader happens to be sitting.
    {
        extraction_context cxt(formats::defaults(), std::nullopt, path::create(".fragment"));
        auto               rdr = open("[]");

        ensure(!cxt.extract<std::int64_t>(rdr).has_value());
        ensure_eq(path::create(".fragment"), cxt.problems().at(0).path());
    }

    {
        extraction_context             cxt(formats::defaults());
        auto                           rdr = open("[]");
        extraction_context::path_scope scope(cxt, std::string_view("renamed"));

        ensure(!cxt.extract<std::int64_t>(rdr).has_value());
        ensure_eq(path::create(".renamed"), cxt.problems().at(0).path());
    }
}

TEST(extract_failure_location_does_not_outlive_its_extraction)
{
    // The `extraction_error` branch folds problems which already carry their own paths, so it never asks where it is
    // -- and must not leave the location the bridge deposited lying around for the next failure to pick up. What this
    // needs is a composite which both materialises and reports by throwing; `optional_adapter` stood in here until it
    // read the reader directly, and the DSL until it did, so the adapter is written against the `value` interface
    // here rather than borrowed from whichever one has not been ported yet.
    extraction_context cxt(bridged_formats());
    auto               rdr = open(R"([ { "a": 1, "b": 2, "c": "x" }, "not a number" ])");
    (void) rdr.next_token();   // onto the object, which is `[0]`

    // The object is materialised, so the cursor moves past it; the nested extraction then throws an
    // `extraction_error`, which is folded rather than re-located.
    ensure(!cxt.extract<bridged_triple>(rdr).has_value());
    auto after_first = cxt.problems().size();
    ensure_gt(after_first, 0U);

    // The cursor is now on `[1]`, and that is where this failure is.
    ensure(!cxt.extract<std::int64_t>(rdr).has_value());
    ensure_gt(cxt.problems().size(), after_first);
    ensure_eq(path::create("[1]"), cxt.problems().back().path());
}

TEST(extract_expect_is_independent_of_a_pending_failure_location)
{
    // `expect` is about where the reader is sitting, which is always right at the moment it is asked. The location a
    // bridge leaves behind on its way out of a failure is for translating that one exception and nothing else.
    extraction_context cxt(bridged_formats());
    auto               rdr = open(R"({ "a": [ [], 99 ] })");

    while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
        (void) rdr.next_token();   // onto the `[` of `.a`
    (void) rdr.next_token();       // onto the inner `[]`, which is `.a[0]`

    // Reaching the extractor through `formats` is public, discouraged, and -- the point here -- skips the handler
    // which would have translated and consumed what the bridge leaves behind.
    alignas(bridged_int) std::byte place[sizeof(bridged_int)];
    try
    {
        (void) cxt.formats().extract(typeid(bridged_int), rdr, static_cast<void*>(place), cxt);
        ensure(!"extracting an integer from an array did not fail");
    }
    catch (const std::exception&)
    { }

    ensure(cxt.problems().empty());

    // The reader is on `99` now. That is where this mismatch is, regardless of what the abandoned extraction left.
    ensure(!cxt.expect(rdr, ast_node_type::string_canonical).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[1]"), cxt.problems().at(0).path());
}

TEST(extract_value_and_text_sources_agree)
{
    std::string source = R"({ "i": 5, "a": [ 1, 2, 3 ], "s": "thing" })";

    ensure_eq(extract<std::int64_t>(parse(source).at_path(".i")), 5);

    extraction_context cxt(formats::defaults());
    auto               rdr = open(source);
    ensure_eq(parse(source), *cxt.extract<value>(rdr));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// extract_options::on_error and max_failures                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(extract_collect_default_stops_at_the_first_problem)
{
    // The default is `fail_immediately` and must stay that way: every existing caller gets one problem and a stop.
    value source = parse(R"({ "a": "x", "b": "y", "c": "z" })");

    try
    {
        (void) extract<triple>(source, triple_formats());
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure_eq(path::create(".a"), err.problems().at(0).path());
    }
}

TEST(extract_collect_all_gathers_every_bad_member)
{
    value source = parse(R"({ "a": "x", "b": 2, "c": "z" })");

    try
    {
        (void) extract<triple>(source, triple_formats(), collecting());
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(2U, err.problems().size());
        ensure_eq(std::string(".a .c"), problem_paths(err));
    }
}

TEST(extract_collect_all_gathers_every_missing_member)
{
    // `mutate` throws for a missing required field as well as for a failed conversion, so recovery reports all of
    // them rather than only the first. The path names the object, so the field is identified by the message.
    try
    {
        (void) extract<triple>(parse("{ }"), triple_formats(), collecting());
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(3U, err.problems().size());
        ensure_eq(std::string("Missing required field a"), err.problems().at(0).message());
        ensure_eq(std::string("Missing required field b"), err.problems().at(1).message());
        ensure_eq(std::string("Missing required field c"), err.problems().at(2).message());
    }
}

TEST(extract_collect_all_gathers_every_bad_element)
{
    try
    {
        (void) extract<std::vector<std::int64_t>>(parse(R"([ 1, "x", 3, "y" ])"), triple_formats(), collecting());
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(2U, err.problems().size());
        ensure_eq(std::string("[1] [3]"), problem_paths(err));
    }
}

TEST(extract_collect_all_resumes_after_a_bridged_element_over_text)
{
    // Over text there is no tree for a bridged adapter to borrow, so the bridge materialises the element -- and
    // materialising is what walks the cursor over it. A failure then leaves the cursor already past the element,
    // where every other failure leaves it *on* the element, so a container recovering with a plain `next_value`
    // steps over the following sibling as well and loses both it and whatever it had to report.
    extraction_context cxt(triple_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
    auto               rdr = open(R"([ { "a": "x", "b": 2, "c": 3 },
                                       { "a": 1,   "b": 2, "c": 3 },
                                       { "a": "y", "b": 2, "c": 3 }
                                     ])"
                                 );

    ensure(!cxt.extract<std::vector<triple>>(rdr).has_value());
    ensure_eq(2U, cxt.problems().size());
    ensure_eq(path::create("[0].a"), cxt.problems().at(0).path());
    ensure_eq(path::create("[2].a"), cxt.problems().at(1).path());
}

TEST(extract_collect_all_records_a_nested_failure_exactly_once)
{
    // The interesting one. `extraction_context::extract` moves the problems it collected off the context and into
    // the exception it throws, so a fold at both the member loop and the element loop would report every failure
    // twice. Two levels of recovery over four failures must still be four problems.
    value source = parse(R"([ { "a": "x", "b": "y", "c": 3 },
                              { "a": 1,   "b": 2,   "c": 3 },
                              { "a": "x", "b": "y", "c": 3 }
                            ])"
                        );

    try
    {
        (void) extract<std::vector<triple>>(source, triple_formats(), collecting());
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(4U, err.problems().size());
        ensure_eq(std::string("[0].a [0].b [2].a [2].b"), problem_paths(err));
    }
}

TEST(extract_collect_all_stops_at_max_failures)
{
    value source = parse(R"([ "a", "b", "c", "d", "e" ])");

    try
    {
        (void) extract<std::vector<std::int64_t>>(source, triple_formats(), collecting(3U));
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(3U, err.problems().size());
        ensure_eq(std::string("[0] [1] [2]"), problem_paths(err));
    }
}

TEST(extract_collect_all_max_failures_boundary_is_fail_immediately)
{
    // A budget of 0 or 1 makes the first problem the last, which is `fail_immediately` in all but name -- and, note,
    // still reports one problem rather than none.
    for (auto limit : { extract_options::size_type(0U), extract_options::size_type(1U) })
    {
        try
        {
            (void) extract<triple>(parse(R"({ "a": "x", "b": "y", "c": "z" })"), triple_formats(), collecting(limit));
            ensure(!"extraction_error was not thrown");
        }
        catch (const extraction_error& err)
        {
            ensure_eq(1U, err.problems().size());
            ensure_eq(path::create(".a"), err.problems().at(0).path());
        }
    }
}

TEST(extract_collect_all_terminates_without_an_enclosing_composite)
{
    // Nothing knows where to resume, so there is no loop to ask and the failure ends extraction. Collecting is
    // something a composite opts into, not something the context can deliver on its own.
    try
    {
        (void) extract<std::int64_t>(parse(R"("x")"), formats::defaults(), collecting());
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(1U, err.problems().size());
    }
}

TEST(extract_collect_all_still_throws_when_it_recovered)
{
    // Recovering collects diagnostics; it does not hand back a half-built object. An array of five where only the
    // third is bad still fails rather than yielding the other four.
    ensure_throws(extraction_error,
                  extract<std::vector<std::int64_t>>(parse(R"([ 1, 2, "x", 4, 5 ])"), triple_formats(), collecting())
                 );
}

TEST(extract_collect_all_recovers_from_a_default_factory_failure)
{
    // A member's default factory is user code running outside the member's own extraction, so it fails with
    // `std::out_of_range` rather than an `extraction_error`. Recovery has to cover that too, or which members get
    // looked at would depend on how the first failing one happened to fail.
    try
    {
        (void) extract<triple>(parse(R"({ "b": "x", "c": "y" })"), seeded_triple_formats(), collecting());
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        // Document order, not declaration order: `a` is absent, so its factory runs in the pass over the members no
        // key claimed, which is after the walk `b` and `c` failed during.
        ensure_eq(3U, err.problems().size());
        ensure_eq(std::string(".b .c .a"), problem_paths(err));
    }
}

TEST(extract_collect_all_max_failures_is_a_threshold_not_a_truncation)
{
    // A single failure which reports several problems at once is folded on whole rather than torn in half, so the
    // final list can exceed the limit by that batch. Pinned because it is the documented behaviour, not an accident:
    // truncating would drop diagnostics to enforce a bound whose point is to stop the walk, not to edit the report.
    try
    {
        (void) extract<batched>(parse("5"), batched_formats(), collecting(2U));
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(3U, err.problems().size());
    }
}

TEST(extract_default_factory_failure_is_untouched_when_not_collecting)
{
    // The new handler declines before recording anything and rethrows the original, so `fail_immediately` sees the
    // same exception reaching the same translation it always did -- with the factory's own exception kept as the
    // cause.
    try
    {
        (void) extract<triple>(parse(R"({ "b": 2, "c": 3 })"), seeded_triple_formats());
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure(err.problems().at(0).nested_ptr());
        ensure_throws(std::out_of_range, (std::rethrow_exception(err.problems().at(0).nested_ptr()), 0));
    }
}

TEST(extract_collect_all_recovered_factory_failure_names_its_member)
{
    // A caller which walked the reader to `.child` itself has told the reader where it is and not the context, and
    // the two are arbitrated by `extraction_context::path_scope`: a live scope is authoritative and the reader is
    // not consulted, so this names the member rather than the document position. That is what makes the recovered
    // problem say which default factory blew up, which the un-recovered path cannot -- it reports where the reader
    // was and not which member was being built.
    value  source = parse(R"({ "child": { "b": 2, "c": 3 } })");
    reader rdr    = reader::from_value(source);

    (void) rdr.next_token();   // onto the outer object
    (void) rdr.next_token();   // onto the key "child"
    (void) rdr.next_token();   // onto the child object itself

    extraction_context cxt(seeded_triple_formats(), std::nullopt, jsonv::path(), nullptr, collecting());

    auto result = cxt.extract<triple>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a"), cxt.problems().at(0).path());
    ensure(cxt.problems().at(0).nested_ptr());
}

TEST(extract_factory_failure_outside_collect_all_still_reports_where_the_reader_was)
{
    // The mirror of the test above: with nothing to recover into, the original exception unwinds to the bridge and
    // is located from the reader, exactly as it was before this handler existed.
    value  source = parse(R"({ "child": { "b": 2, "c": 3 } })");
    reader rdr    = reader::from_value(source);

    (void) rdr.next_token();
    (void) rdr.next_token();
    (void) rdr.next_token();

    extraction_context cxt(seeded_triple_formats());

    auto result = cxt.extract<triple>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".child"), cxt.problems().at(0).path());
}

TEST(extract_context_carries_its_options)
{
    extraction_context defaulted(formats::defaults());
    ensure(defaulted.options().failure_mode() == extract_options::on_error::fail_immediately);
    ensure(!defaulted.recover());

    extraction_context collector(formats::defaults(), std::nullopt, jsonv::path(), nullptr, collecting(2U));
    ensure(collector.options().failure_mode() == extract_options::on_error::collect_all);

    // Nothing recorded yet, so there is room; after two there is not.
    ensure(collector.recover());
    (void) collector.problem(jsonv::path(), "first");
    ensure(collector.recover());
    (void) collector.problem(jsonv::path(), "second");
    ensure(!collector.recover());
}

}
