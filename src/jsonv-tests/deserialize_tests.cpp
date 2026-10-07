/// \file
/// Tests for the reader-based \c deserializer interface.
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
#include <jsonv/serialization/function_deserializer.hpp>
#include <jsonv/value.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <memory>
#include <new>
#include <set>
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

/// A deserializer written directly against the `void*` interface, which is the thing most implementations should not
/// have to do -- so it is the thing worth having a test for.
class bounded_int_deserializer final :
        public deserializer
{
public:
    const std::type_info& get_type() const noexcept override
    {
        return typeid(std::int64_t);
    }

    std::expected<void, ast_node_type>
    deserialize(deserialization_context& context, reader& from, void* into) const override
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
    static bounded_int_deserializer instance;

    formats out = formats::compose({ formats::defaults() });
    out.register_deserializer(&instance, duplicate_type_action::replace);
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

/// A `triple` whose `a` falls back to a factory which fails -- user code running outside the member's own
/// deserialization, so it fails with whatever it threw rather than with a `deserialization_error`.
formats seeded_triple_formats()
{
    static formats instance =
        formats_builder()
            .type<triple>()
                .member("a", &triple::a)
                    .default_value([] (deserialization_context&) -> std::int64_t
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
/// once does. `max_failures` is a threshold deserialization stops at, not a truncation applied to what a single failure
/// reports, so a batch like this arrives whole.
struct batched { };

class batch_problem_adapter final :
        public adapter_for<batched>
{
protected:
    std::expected<batched, ast_node_type> create(deserialization_context& context, reader& from) const override
    {
        (void) from.next_value();

        (void) context.problem(context.path(), "first");
        (void) context.problem(context.path(), "second");
        return context.problem(context.path(), "third");
    }

    void serialize(const serialization_context&, const batched&, writer& to) const override
    {
        to.null();
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
/// did before they read the reader directly: by letting `value::as_integer` throw. The bridge is still how every
/// adapter written against `value_adapter_for` reaches a value, so what it does with a failure is still worth pinning.
struct bridged_int
{
    std::int64_t value;
};

class bridged_int_adapter final :
        public value_adapter_for<bridged_int>
{
protected:
    bridged_int create(deserialization_context&, const value& from) const override
    {
        return bridged_int{ from.as_integer() };
    }

    value to_json(const serialization_context&, const bridged_int& from) const override
    {
        return value(from.value);
    }
};

/// A type whose member is a view of whatever it was deserialized from, which is only valid while that source is. Used
/// from both sides of the materialisation question, so it lives out here rather than inside either test.
struct view_holder
{
    std::string_view s;
};

/// Three members reached through the older `value`-based interface. Materialising the object is what walks the cursor
/// past it and looking each member up by name is what needs the whole tree, so this is the shape the DSL had before it
/// read the reader -- kept here because the bridge is still how every adapter written against `value_adapter_for`
/// reaches a value, and what it does with a failure is still worth pinning.
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
    bridged_triple create(deserialization_context& context, const value& from) const override
    {
        bridged_triple out;
        out.a = context.deserialize<std::int64_t>(from.at("a"));
        out.b = context.deserialize<std::int64_t>(from.at("b"));
        out.c = context.deserialize<std::int64_t>(from.at("c"));
        return out;
    }

    value to_json(const serialization_context&, const bridged_triple&) const override
    {
        return object();
    }
};

/// Reached through the older `value`-based interface, and refusing to move when it holds 9. Its adapter returns it as a
/// prvalue, so nothing moves it before the bridge's own return does -- after the bridge has committed its subtree.
struct bridged_fragile
{
    std::int64_t v = 0;

    bridged_fragile()                                  = default;
    bridged_fragile(const bridged_fragile&)            = default;
    bridged_fragile& operator=(const bridged_fragile&) = default;
    bridged_fragile& operator=(bridged_fragile&&)      = default;

    explicit bridged_fragile(std::int64_t v) :
            v(v)
    { }

    bridged_fragile(bridged_fragile&& other) :
            v(other.v)
    {
        if (v == 9)
            throw std::runtime_error("move refuses");
    }
};

class bridged_fragile_adapter final :
        public value_adapter_for<bridged_fragile>
{
protected:
    bridged_fragile create(deserialization_context& context, const value& from) const override
    {
        return bridged_fragile(context.deserialize<std::int64_t>(from.at("v")));
    }

    value to_json(const serialization_context&, const bridged_fragile&) const override
    {
        return object();
    }
};

formats bridged_formats()
{
    static bridged_int_adapter     int_instance;
    static bridged_triple_adapter  triple_instance;
    static bridged_fragile_adapter fragile_instance;

    formats out = formats::compose({ formats::defaults() });
    out.register_adapter(&int_instance);
    out.register_adapter(&triple_instance);
    out.register_adapter(&fragile_instance);
    return out;
}

deserialize_options collecting(deserialize_options::size_type max_failures = 10U)
{
    return deserialize_options::create_default()
                .failure_mode(deserialize_options::on_error::collect_all)
                .max_failures(max_failures);
}

/// The paths of every problem a `deserialization_error` carries, in document order and joined, so a mismatch prints
/// the whole list rather than the first difference.
std::string problem_paths(const deserialization_error& err)
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
void with_nested_path_scopes(deserialization_context& cxt, std::size_t depth, const FAtBottom& at_bottom)
{
    if (depth == 0U)
    {
        at_bottom();
    }
    else if (depth % 2U == 0U)
    {
        deserialization_context::path_scope scope(cxt, depth);
        with_nested_path_scopes(cxt, depth - 1U, at_bottom);
    }
    else
    {
        // A literal, so it outlives the scope viewing it -- which is the requirement the forms which view a key put on
        // their caller in exchange for not copying it.
        deserialization_context::path_scope scope(cxt, "k");
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

/// \c named_triple with a \c post_deserialize which validates the object without quoting it, as every one does on the
/// success path.
struct quiet_triple
{
    std::int64_t a;
    std::int64_t b;
    std::int64_t c;
};

/// \c named_triple with a \c post_deserialize which quotes the object, which read from a \c value means encoding it.
struct quoting_triple
{
    std::int64_t a;
    std::int64_t b;
    std::int64_t c;
};

/// \c named_triple with a \c post_deserialize which asks for the object as a \c value, which read from text means
/// reading it again.
struct asking_triple
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

/// The DSL's key loop with the `path_scope` taken out and not one other thing changed -- bar the `source_scope`, which
/// records where the object is and so has nothing to allocate either. It was written against the `value` interface
/// while the DSL was, and had to follow it onto the reader: an adapter which materialises has a wholly different cost,
/// so comparing one against the other would measure the materialisation and not the naming.
class unnamed_triple_adapter final :
        public adapter_for<unnamed_triple>
{
protected:
    JSONV_NODISCARD
    virtual std::expected<unnamed_triple, ast_node_type> create(deserialization_context& context,
                                                                reader&             from
                                                               ) const override
    {
        auto opened = context.current_as<ast_node::object_begin>(from);
        if (!opened)
            return std::unexpected(opened.error());

        unnamed_triple out;

        (void) from.next_token();
        while (from.good() && from.current_type() != ast_node_type::object_end)
        {
            auto key = from.current().as<ast_node::key_canonical>().value();
            if (!from.next_token())
                break;

            auto member = context.deserialize<std::int64_t>(from);
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

    virtual void serialize(const serialization_context&, const unnamed_triple&, writer& to) const override
    {
        to.null();
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
    virtual std::expected<unnamed_vector, ast_node_type> create(deserialization_context& context,
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
            if (from.current_type() == ast_node_type::array_end)
            {
                (void) from.next_token();
                return out;
            }

            auto element = context.deserialize<std::int64_t>(from);
            if (!element)
                return std::unexpected(element.error());

            out.values.insert(end(out.values), *std::move(element));
        }

        return context.problem(context.problem_path(from), "Unterminated array");
    }

    virtual void serialize(const serialization_context&, const unnamed_vector&, writer& to) const override
    {
        to.array_begin().array_end();
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
                                .type<quiet_triple>()
                                    .member(long_a, &quiet_triple::a)
                                    .member(long_b, &quiet_triple::b)
                                    .member(long_c, &quiet_triple::c)
                                    .post_deserialize([] (const deserialization_context&, quiet_triple&& out)
                                                      {
                                                          if (out.a < 0)
                                                              throw std::invalid_argument("a must not be negative");

                                                          return out;
                                                      }
                                                     )
                                .type<quoting_triple>()
                                    .member(long_a, &quoting_triple::a)
                                    .member(long_b, &quoting_triple::b)
                                    .member(long_c, &quoting_triple::c)
                                    .post_deserialize([] (const deserialization_context& context, quoting_triple&& out)
                                                      {
                                                          (void) context.encoded_source();
                                                          return out;
                                                      }
                                                     )
                                .type<asking_triple>()
                                    .member(long_a, &asking_triple::a)
                                    .member(long_b, &asking_triple::b)
                                    .member(long_c, &asking_triple::c)
                                    .post_deserialize([] (const deserialization_context& context, asking_triple&& out)
                                                      {
                                                          (void) context.source_value();
                                                          return out;
                                                      }
                                                     )
                                .register_container<std::vector<named_triple>>()
                                .register_container<std::vector<quiet_triple>>()
                                .register_container<std::vector<quoting_triple>>()
                                .register_container<std::vector<asking_triple>>()
                                .register_container<std::vector<std::int64_t>>()
                                .register_container<std::vector<double>>()
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

/// An array of \a count decimals, none of them integral, so that each one formats to a token of some length.
value decimals(std::size_t count)
{
    value out = array();
    for (std::size_t idx = 0U; idx < count; ++idx)
        out.push_back(double(idx) + 0.123456789);

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

/// The allocations a fresh deserialization of \c T from \a source performs. The result's size is checked too, so a
/// deserialization which quietly produced nothing cannot pass for a cheap one.
template <typename T>
std::size_t deserialization_cost(const value& source)
{
    deserialization_context cxt(path_cost_formats());

    allocation_counter allocations;
    const auto         out  = cxt.deserialize<T>(source);
    const std::size_t  cost = allocations.count();

    ensure_eq(source.size(), out.size());
    return cost;
}

/// \ref deserialization_cost for JSON \a text, an array of \a count elements. Parsing the text is not deserialization,
/// so the reader is made before the count starts.
template <typename T>
std::size_t deserialization_cost(std::string_view text, std::size_t count)
{
    deserialization_context cxt(path_cost_formats());
    reader             rdr(text);

    allocation_counter allocations;
    const auto         out  = deserialize<T>(rdr, cxt);
    const std::size_t  cost = allocations.count();

    ensure_eq(count, out.size());
    return cost;
}

#endif

}

TEST(deserialize_reader_deserializer_success)
{
    deserialization_context cxt(bounded_formats());
    auto               rdr = open("1234");

    auto result = cxt.deserialize<std::int64_t>(rdr);
    ensure(result.has_value());
    ensure_eq(1234, *result);
    ensure(cxt.problems().empty());
}

TEST(deserialize_reader_deserializer_problem)
{
    deserialization_context cxt(bounded_formats());
    auto               rdr = open("7");

    auto result = cxt.deserialize<std::int64_t>(rdr);
    ensure(!result.has_value());
    ensure(result.error() == ast_node_type::error);
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Expected a value between 500 and 2500"), cxt.problems().at(0).message());
}

TEST(deserialize_reader_deserializer_type_mismatch_carries_the_type_found)
{
    deserialization_context cxt(bounded_formats());
    auto               rdr = open(R"("not a number")");

    auto result = cxt.deserialize<std::int64_t>(rdr);
    ensure(!result.has_value());

    // A mismatch reports what was actually there rather than the `error` sentinel.
    ensure(result.error() == ast_node_type::string_canonical);
    ensure_eq(1U, cxt.problems().size());
}

TEST(deserialize_context_expect_single_type)
{
    deserialization_context cxt;
    auto               rdr = open("true");

    auto matched = cxt.expect(rdr, ast_node_type::integer);
    ensure(!matched.has_value());
    ensure(matched.error() == ast_node_type::literal_true);
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Read node of type true when expecting integer"), cxt.problems().at(0).message());

    ensure(cxt.expect(rdr, ast_node_type::literal_true).has_value());
    ensure_eq(1U, cxt.problems().size());
}

TEST(deserialize_context_expect_type_list)
{
    deserialization_context cxt;
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

TEST(deserialize_context_expect_reports_the_readers_path)
{
    deserialization_context cxt;
    auto               rdr = open(R"({ "a": [ 1, "two" ] })");

    // Walk to the "two" so the reader has a non-trivial path to report the problem at.
    while (rdr.good() && rdr.current().type() != ast_node_type::string_canonical)
        (void) rdr.next_token();

    ensure(!cxt.expect(rdr, ast_node_type::integer).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[1]"), cxt.problems().at(0).path());
}

TEST(deserialize_context_current_as)
{
    deserialization_context cxt;
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

TEST(deserialize_path_scope_restores_on_exit)
{
    deserialization_context cxt;
    ensure_eq(path(), cxt.path());

    {
        deserialization_context::path_scope outer(cxt, "a");
        ensure_eq(path::create(".a"), cxt.path());

        {
            deserialization_context::path_scope inner(cxt, std::size_t(3));
            ensure_eq(path::create(".a[3]"), cxt.path());
        }

        ensure_eq(path::create(".a"), cxt.path());
    }

    ensure_eq(path(), cxt.path());
}

TEST(deserialize_path_scope_restores_when_unwound_by_an_exception)
{
    deserialization_context cxt;

    try
    {
        deserialization_context::path_scope outer(cxt, "a");
        deserialization_context::path_scope inner(cxt, path_element(std::string("owned")));
        ensure_eq(path::create(".a.owned"), cxt.path());
        throw std::runtime_error("unwind");
    }
    catch (const std::runtime_error&)
    { }

    ensure_eq(path(), cxt.path());

    // An `_innermost` left naming a destroyed frame would report whatever that stack now holds rather than just
    // `.after`. ASan is where that shows up, as a stack-use-after-scope rather than a quietly wrong answer.
    deserialization_context::path_scope after(cxt, "after");
    ensure_eq(path::create(".after"), cxt.path());
}

TEST(deserialize_path_scope_names_a_key_however_it_is_spelt)
{
    // Before #299 a literal and a `std::string` both reached the `std::string_view` and `path_element` overloads
    // through one conversion apiece, so the two most natural ways of naming a member did not compile.
    deserialization_context cxt;

    {
        deserialization_context::path_scope scope(cxt, "literal");
        ensure_eq(path::create(".literal"), cxt.path());
    }

    {
        // Viewed rather than copied, so an edit made in place shows through.
        std::string                    key = "lvalue";
        deserialization_context::path_scope scope(cxt, key);
        key[0] = 'L';
        ensure_eq(path::create(".Lvalue"), cxt.path());
    }

    {
        const std::string              key = "constant";
        deserialization_context::path_scope scope(cxt, key);
        ensure_eq(path::create(".constant"), cxt.path());
    }

    {
        // Owned, since the temporary is gone by the next line. It is too long for any small-string buffer, so a scope
        // which viewed it would be reading freed memory -- which ASan reports rather than leaving to chance.
        deserialization_context::path_scope scope(cxt, std::string("temporary_with_a_deliberately_long_name"));
        ensure_eq(path::create(".temporary_with_a_deliberately_long_name"), cxt.path());
    }

    {
        // Owned as well, so what the caller goes on to do with the string does not reach the scope. Were it viewed,
        // this would read ".other".
        std::string                    key = "moved";
        deserialization_context::path_scope scope(cxt, std::move(key));
        key.assign("other");
        ensure_eq(path::create(".moved"), cxt.path());
    }

    {
        // A `const` temporary cannot be moved from, so it is copied -- but still owned rather than viewed, which ASan
        // would report as soon as the path was read.
        const auto make_key = [] () -> const std::string { return "constant_temporary_with_a_deliberately_long_name"; };

        deserialization_context::path_scope scope(cxt, make_key());
        ensure_eq(path::create(".constant_temporary_with_a_deliberately_long_name"), cxt.path());
    }

    {
        // An array of `char` about to be destroyed is copied for the same reason. Were it viewed, this would read
        // ".Array".
        char                           key[] = "array";
        deserialization_context::path_scope scope(cxt, std::move(key));
        key[0] = 'A';
        ensure_eq(path::create(".array"), cxt.path());
    }

    {
        std::string_view               key = "view";
        deserialization_context::path_scope scope(cxt, key);
        ensure_eq(path::create(".view"), cxt.path());
    }

    {
        // A braced `{ data, size }` initializes a `std::string` as readily as a `std::string_view`. It goes on reaching
        // the `std::string_view` overload alone because those taking a `std::string` are templates it cannot deduce.
        const char*                    data = "braced_and_then_some";
        deserialization_context::path_scope scope(cxt, { data, 6U });
        ensure_eq(path::create(".braced"), cxt.path());
    }

    {
        // An index on every compiler. Taking a literal as a `const char*` would have made this ambiguous, but only on
        // GCC.
        deserialization_context::path_scope scope(cxt, 0);
        ensure_eq(path::create("[0]"), cxt.path());
    }

    ensure_eq(path(), cxt.path());
}

TEST(deserialize_path_scope_takes_precedence_over_the_readers_path)
{
    deserialization_context cxt;
    auto               rdr = open(R"({ "a": "wrong" })");

    while (rdr.good() && rdr.current().type() != ast_node_type::string_canonical)
        (void) rdr.next_token();

    // The reader would say ".a"; the scope says where the *deserializer* thinks it is, and wins.
    deserialization_context::path_scope scope(cxt, "renamed");
    ensure(!cxt.expect(rdr, ast_node_type::integer).has_value());
    ensure_eq(path::create(".renamed"), cxt.problems().at(0).path());
}

TEST(deserialize_member_scope_ends_before_the_setter)
{
    // The member's scope names where the *deserialization* is, not where the assignment is. A setter supplied through
    // the `member(name, access, mutate)` overload is arbitrary user code, and once the member's value exists the
    // deserializer is no longer at that key -- anything the setter goes on to deserialize sits where the document puts
    // it rather than underneath the member whose setter happens to be running. `pre_deserialize` is handed the
    // context precisely so user code can reach it, which is what lets this observe the boundary at all.
    struct probe
    {
        std::int64_t a;
    };

    deserialization_context* captured = nullptr;
    jsonv::path         seen_by_setter = path::create(".neverran");

    const formats fmts =
        formats_builder()
            .type<probe>()
                .pre_deserialize([&captured] (deserialization_context& cxt) { captured = &cxt; })
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

    const auto out = deserialize<probe>(parse(R"({ "a": 7 })"), fmts);

    ensure_eq(7, out.a);
    ensure_eq(path(), seen_by_setter);
}

TEST(deserialize_allocation_counting_is_available)
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

TEST(deserialize_path_scope_push_and_pop_allocate_nothing)
{
    // The path is read only when something goes wrong, so maintaining it has to be free when nothing does: a scope
    // is a stack object linked into a chain, and descending into an element costs two stores rather than a copy of
    // everything above it. A regression here would otherwise surface only as a slightly worse benchmark number.
    deserialization_context cxt;

    for (std::size_t depth : { std::size_t(1U), std::size_t(64U), std::size_t(1024U) })
    {
        allocation_counter allocations;
        with_nested_path_scopes(cxt, depth, [] { });
        const std::size_t cost = allocations.count();

        ensure_eq(0U, cost);
        ensure(cxt.path().empty());
    }

    // A `std::string` is viewed just as a `std::string_view` is, so naming a key too long for any small-string buffer
    // costs nothing either.
    {
        allocation_counter allocations;
        {
            deserialization_context::path_scope scope(cxt, long_a);
        }
        const std::size_t cost = allocations.count();

        ensure_eq(0U, cost);
    }

    // The chain still says where it is. Asked outside a counted region, because building a `jsonv::path` is exactly
    // the work this design defers to the moment something actually fails.
    jsonv::path deepest;
    with_nested_path_scopes(cxt, 3U, [&] { deepest = cxt.path(); });
    ensure_eq(path::create(".k[2].k"), deepest);
}

TEST(deserialize_container_element_naming_costs_nothing)
{
    // The same array through two adapters which differ only in whether the element is named. Everything else -- the
    // context, the reader the bridge builds per element, the deserialization itself, the vector it lands in and how
    // that vector grows -- is identical and cancels, so the two counts are equal exactly when naming is free. Before
    // this change `container_adapter` reached the scope through `extract_sub`, which builds a `jsonv::path` from its
    // argument, and the named arm exceeded the unnamed one by one allocation per element.
    const value source = integers(256U);

    // Discarded: the first deserialization of a type pays for whatever the formats cache on first use, which is not
    // per-element and not what is being compared.
    (void) deserialization_cost<std::vector<std::int64_t>>(source);
    (void) deserialization_cost<unnamed_vector>(source);

    const std::size_t named   = deserialization_cost<std::vector<std::int64_t>>(source);
    const std::size_t unnamed = deserialization_cost<unnamed_vector>(source);

    ensure_eq(unnamed, named);
}

TEST(deserialize_member_naming_costs_nothing)
{
    // The container half of this is the same on both sides, so what is left is the DSL's member loop against an
    // adapter reading the same three members without naming them. The names are long on purpose: the old code
    // copied the key into a `path_element`, which a short name hides inside the small-string buffer.
    const value source = named_triples(64U);

    (void) deserialization_cost<std::vector<named_triple>>(source);
    (void) deserialization_cost<std::vector<unnamed_triple>>(source);

    const std::size_t named   = deserialization_cost<std::vector<named_triple>>(source);
    const std::size_t unnamed = deserialization_cost<std::vector<unnamed_triple>>(source);

    ensure_eq(unnamed, named);
}

TEST(deserialize_encoded_source_costs_nothing_unless_asked)
{
    // Read from a `value`, there is no text for `encoded_source` to view, so quoting an object means encoding it. That
    // waits for a hook to ask: one which validates without quoting, which is what every hook does when nothing is
    // wrong, costs what having no hook costs. The hook which does ask is what shows the comparison can see an encoding
    // -- and it has to be dearer by one per object, since every one of them encodes past the small-string buffer. An
    // encoding made for every object whether asked for or not would be in all three, and fail only that.
    const value source = named_triples(16U);

    (void) deserialization_cost<std::vector<named_triple>>(source);
    (void) deserialization_cost<std::vector<quiet_triple>>(source);
    (void) deserialization_cost<std::vector<quoting_triple>>(source);

    const std::size_t plain = deserialization_cost<std::vector<named_triple>>(source);

    ensure_eq(plain, deserialization_cost<std::vector<quiet_triple>>(source));
    ensure_le(plain + source.size(), deserialization_cost<std::vector<quoting_triple>>(source));
}

TEST(deserialize_source_value_costs_nothing_unless_asked)
{
    // Read from text, a hook which asks for `source_value` after the walk has the object read again, from a position on
    // the tape kept from its `{`. That position is kept for every object, whether anything goes on to ask or not, so it
    // has to cost nothing: the DSL's walk costs what a hand-written one with no scope at all costs, and a hook which
    // never asks costs what no hook does. The hook which does ask is what shows the comparison can see a read, which
    // allocates at least once per object for the second cursor alone.
    const std::size_t count = 16U;
    const std::string text  = to_string(named_triples(count));

    (void) deserialization_cost<std::vector<named_triple>>(text, count);
    (void) deserialization_cost<std::vector<unnamed_triple>>(text, count);
    (void) deserialization_cost<std::vector<quiet_triple>>(text, count);
    (void) deserialization_cost<std::vector<asking_triple>>(text, count);

    const std::size_t plain = deserialization_cost<std::vector<named_triple>>(text, count);

    ensure_eq(deserialization_cost<std::vector<unnamed_triple>>(text, count), plain);
    ensure_eq(plain, deserialization_cost<std::vector<quiet_triple>>(text, count));
    ensure_le(plain + count, deserialization_cost<std::vector<asking_triple>>(text, count));
}

TEST(deserialize_numbers_from_a_value_synthesise_no_tokens)
{
    // A value-backed reader has to format a number into its arena before it can hand out an `ast_node` for it --
    // which the deserializers then ignore (a `double`) or parse straight back (an integer). Nothing between a `value`
    // and a container of numbers needs more than the node's type, so four thousand elements have to cost what four
    // do. Before #238 the arena grew a chunk for every few hundred numbers.
    (void) deserialization_cost<std::vector<std::int64_t>>(integers(4U));
    (void) deserialization_cost<std::vector<double>>(decimals(4U));

    ensure_eq(deserialization_cost<std::vector<std::int64_t>>(integers(4U)),
              deserialization_cost<std::vector<std::int64_t>>(integers(4096U))
             );
    ensure_eq(deserialization_cost<std::vector<double>>(decimals(4U)),
              deserialization_cost<std::vector<double>>(decimals(4096U))
             );
}

TEST(deserialize_path_tracking_does_not_grow_with_the_base_path)
{
    // A context deserializing a fragment of some larger document carries the path to that fragment. `_base_path` is
    // read only by `path()`, which a successful deserialization never calls, so how deep the fragment sits changes
    // nothing. This is the guard against the design this one replaced, where a scope saved and restored a copy of
    // the whole current path and every push would have copied those five elements and their five keys.
    const value source = named_triples(16U);

    auto cost_with = [&source] (jsonv::path base) -> std::size_t
                     {
                         deserialization_context cxt(path_cost_formats(), std::nullopt, std::move(base));

                         allocation_counter allocations;
                         const auto         out  = cxt.deserialize<std::vector<named_triple>>(source);
                         const std::size_t  cost = allocations.count();

                         ensure_eq(source.size(), out.size());
                         return cost;
                     };

    (void) cost_with(jsonv::path());

    ensure_eq(cost_with(jsonv::path()), cost_with(path::create(".a.b.c.d.e")));
}

TEST(deserialize_source_name_costs_nothing_on_success)
{
    // The name is copied onto a problem when one is recorded, and at no other time, so naming the source of a document
    // which deserializes cleanly costs nothing. The name is built outside the counted region, as the base path above
    // is.
    const value source = named_triples(16U);

    auto cost_with = [&source] (std::string name) -> std::size_t
                     {
                         deserialization_context cxt(path_cost_formats(),
                                                     std::nullopt,
                                                     jsonv::path(),
                                                     nullptr,
                                                     deserialize_options(),
                                                     std::move(name)
                                                    );

                         allocation_counter allocations;
                         const auto         out  = cxt.deserialize<std::vector<named_triple>>(source);
                         const std::size_t  cost = allocations.count();

                         ensure_eq(source.size(), out.size());
                         return cost;
                     };

    (void) cost_with(std::string());

    ensure_eq(cost_with(std::string()),
              cost_with("a source name long enough to need the heap rather than the small-string buffer.json")
             );
}

#endif

TEST(deserialize_read_value_scalars)
{
    ensure_eq(value(true),      [] { auto r = open("true");     return read_value(r); }());
    ensure_eq(value(false),     [] { auto r = open("false");    return read_value(r); }());
    ensure_eq(value(),          [] { auto r = open("null");     return read_value(r); }());
    ensure_eq(value(42),        [] { auto r = open("42");       return read_value(r); }());
    ensure_eq(value(4.5),       [] { auto r = open("4.5");      return read_value(r); }());
    ensure_eq(value("thing"),   [] { auto r = open(R"("thing")"); return read_value(r); }());
    ensure_eq(value("a\"b"),    [] { auto r = open(R"("a\"b")"); return read_value(r); }());
}

TEST(deserialize_read_value_structures)
{
    auto empty_object = open("{}");
    ensure_eq(object(), read_value(empty_object));

    auto empty_array = open("[]");
    ensure_eq(array(), read_value(empty_array));

    auto nested = open(R"({ "a": [ 1, { "b": null } ], "c": "d" })");
    ensure_eq(parse(R"({ "a": [ 1, { "b": null } ], "c": "d" })"), read_value(nested));
}

TEST(deserialize_read_value_works_on_a_fresh_reader)
{
    // No explicit step off document_start -- read_value does it.
    reader rdr(R"([ 1, 2 ])");
    ensure_eq(parse("[1, 2]"), read_value(rdr));
}

TEST(deserialize_read_value_lands_one_past_the_subtree)
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

TEST(deserialize_read_value_lands_one_past_an_object_member)
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

TEST(deserialize_read_value_failure_lands_one_past_the_structure)
{
    // A structure which fails part-way through is walked to its end anyway. Left inside it, the cursor would have the
    // next closing token belong to the structure, and whatever recovers from the failure would take it for its own.
    auto check = [](std::string_view source)
                 {
                     auto rdr = open(source);
                     (void) rdr.next_token();

                     ensure_throws(std::invalid_argument, read_value(rdr));

                     ensure(rdr.good());
                     ensure(rdr.current().type() == ast_node_type::integer);
                     ensure_eq(99, rdr.current().as<ast_node::integer>().value());
                 };

    check("[ [ 1e400 ], 99 ]");
    check(R"([ [ 1, 1e400, [ 2 ], { "a": 3 } ], 99 ])");
    check(R"([ { "a": { "b": [ 1e400, 2 ] }, "c": [] }, 99 ])");

    // A scalar is converted before it is stepped over, so one which fails is still under the cursor.
    auto rdr = open("[ 1e400, 99 ]");
    (void) rdr.next_token();
    ensure_throws(std::invalid_argument, read_value(rdr));
    ensure(rdr.current().type() == ast_node_type::decimal);
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

TEST(deserialize_read_value_failing_to_start_a_structure_still_passes_it)
{
    // The first thing reading a structure costs is the empty `value` it is read into. The overload taking a context
    // says a structure which failed is behind the cursor, which is what a composite recovering from it relies on to
    // skip it -- so a failure that early has to leave it behind as well, or the same child is read again as the next.
    for (std::string_view source : { "[ [ 1 ], 2 ]", R"([ { "a": 1 }, 2 ])" })
    {
        deserialization_context cxt(formats::defaults());
        auto               rdr = open(source);
        (void) rdr.next_token();   // onto the child

        {
            jsonv_test::failing_allocation fail;
            ensure_throws(std::bad_alloc, read_value(cxt, rdr));
        }

        cxt.skip_failed_value(rdr);
        ensure(rdr.good());
        ensure(rdr.current().type() == ast_node_type::integer);
        ensure_eq(2, rdr.current().as<ast_node::integer>().value());
    }
}

#endif

TEST(deserialize_read_value_lands_one_past_on_a_value_sourced_reader)
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

TEST(deserialize_read_value_rejects_a_non_value)
{
    auto rdr = open(R"({ "a": 1 })");
    (void) rdr.next_token();   // onto the key, which is not a value
    ensure_throws(deserialization_error, read_value(rdr));
}

TEST(deserialize_value_from_text_settles_a_repeated_key_like_parse)
{
    // `parse` settles a repeated key by `deserialize_options::on_duplicate_key`. Reading the same document through a
    // reader has to agree, at every depth, or the two routes to one `value` hold different data. The repeat `ignore`
    // passes over is stepped over whole, so the `1e400` in it is never read.
    using action = deserialize_options::duplicate_key_action;

    const std::string_view text = R"({ "x": { "a": 1, "a": 2 }, "y": [ { "b": 3, "b": [ 1e400 ] } ] })";
    const std::string_view kept = R"({ "x": { "a": 1 }, "y": [ { "b": 3 } ] })";

    auto options = deserialize_options::create_default().on_duplicate_key(action::ignore);

    deserialization_context cxt(formats::defaults(), std::nullopt, jsonv::path(), nullptr, options);
    auto               rdr = open(text);

    auto out = cxt.deserialize<value>(rdr);
    ensure(cxt.problems().empty());
    ensure_eq(parse(kept), *out);

    const std::string_view replaced = R"({ "x": { "a": 1, "a": 2 }, "y": [ { "b": 3, "b": 4 } ] })";
    for (auto on_duplicate : { action::replace, action::ignore })
    {
        auto settled = deserialize_options::create_default().on_duplicate_key(on_duplicate);

        deserialization_context settled_cxt(formats::defaults(), std::nullopt, jsonv::path(), nullptr, settled);
        auto               settled_rdr = open(replaced);

        auto settled_out = settled_cxt.deserialize<value>(settled_rdr);
        ensure(settled_out.has_value());
        ensure_eq(parse(replaced, parse_options(), settled), *settled_out);
    }
}

TEST(deserialize_value_from_text_refuses_a_repeated_key_under_exception)
{
    // The message is the one `parse` and the serialization builder DSL use for the same document. It is placed at the
    // key which repeated.
    const std::string_view text    = R"({ "x": { "a": 1, "a": 2 } })";
    auto                   options = deserialize_options::create_default()
                                         .on_duplicate_key(deserialize_options::duplicate_key_action::exception);

    deserialization_context cxt(formats::defaults(), std::nullopt, jsonv::path(), nullptr, options);
    auto               rdr = open(text);

    ensure(!cxt.deserialize<value>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string(R"(Duplicate key in object: "a")"), cxt.problems().at(0).message());
    ensure_eq(path::create(".x.a"), cxt.problems().at(0).path());

    ensure_throws(deserialization_error, parse(text, parse_options(), options));
}

TEST(deserialize_value_repeated_key_is_placed_where_the_context_says)
{
    // A context with a location of its own -- a base path, or a scope saying where the deserializer is -- is
    // authoritative over the reader's path, which only knows the fragment it was handed. The part below where the read
    // began is still the reader's to say, which is what names the key.
    const auto options = deserialize_options::create_default()
                             .on_duplicate_key(deserialize_options::duplicate_key_action::exception);

    auto placed = [&options] (std::string_view fragment, jsonv::path base, std::optional<std::string_view> scope)
                  {
                      deserialization_context                            cxt(formats::defaults(),
                                                                             std::nullopt,
                                                                             std::move(base),
                                                                             nullptr,
                                                                             options
                                                                            );
                      std::optional<deserialization_context::path_scope> named;
                      if (scope)
                          named.emplace(cxt, *scope);

                      auto rdr = open(fragment);
                      ensure(!cxt.deserialize<value>(rdr).has_value());
                      ensure_eq(1U, cxt.problems().size());
                      return cxt.problems().at(0).path();
                  };

    const std::string_view flat   = R"({ "a": 1, "a": 2 })";
    const std::string_view nested = R"({ "x": [ 0, { "a": 1, "a": 2 } ] })";

    ensure_eq(path::create(".payload.a"),       placed(flat,   path::create(".payload"), std::nullopt));
    ensure_eq(path::create(".payload.x[1].a"),  placed(nested, path::create(".payload"), std::nullopt));
    ensure_eq(path::create(".renamed.a"),       placed(flat,   jsonv::path(),            "renamed"));
    ensure_eq(path::create(".renamed.x[1].a"),  placed(nested, jsonv::path(),            "renamed"));

    // With no location of its own, the reader's path is the whole answer.
    ensure_eq(path::create(".a"),               placed(flat,   jsonv::path(),            std::nullopt));
    ensure_eq(path::create(".x[1].a"),          placed(nested, jsonv::path(),            std::nullopt));
}

TEST(deserialize_bridge_from_text_settles_a_repeated_key_by_the_options)
{
    // An adapter on the `value` bridge is handed the tree the bridge built, so whichever value of a repeated key that
    // tree kept is the one the adapter sees.
    using action = deserialize_options::duplicate_key_action;

    const std::string_view text = R"({ "a": 1, "b": 2, "c": 3, "a": 4 })";
    for (auto [on_duplicate, expected] : { std::pair(action::replace, std::int64_t(4)),
                                           std::pair(action::ignore,  std::int64_t(1)),
                                         }
        )
    {
        auto options = deserialize_options::create_default().on_duplicate_key(on_duplicate);

        deserialization_context cxt(bridged_formats(), std::nullopt, jsonv::path(), nullptr, options);
        auto               rdr = open(text);

        auto out = cxt.deserialize<bridged_triple>(rdr);
        ensure(out.has_value());
        ensure_eq(expected, out->a);
    }

    auto options = deserialize_options::create_default().on_duplicate_key(action::exception);

    deserialization_context cxt(bridged_formats(), std::nullopt, jsonv::path(), nullptr, options);
    auto               rdr = open(text);

    ensure(!cxt.deserialize<bridged_triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string(R"(Duplicate key in object: "a")"), cxt.problems().at(0).message());

    // Placed under the context's own location, as for any deserializer reading a `value`.
    deserialization_context based_cxt(bridged_formats(), std::nullopt, path::create(".payload"), nullptr, options);
    auto               based_rdr = open(text);

    ensure(!based_cxt.deserialize<bridged_triple>(based_rdr).has_value());
    ensure_eq(1U, based_cxt.problems().size());
    ensure_eq(path::create(".payload.a"), based_cxt.problems().at(0).path());
}

TEST(deserialize_collect_all_resumes_after_a_repeated_key)
{
    // Refusing a repeated key fails part-way through an object, which is walked to its end before the failure gets
    // out -- so a collecting container resumes at the next element, not inside the object and not one past it.
    auto options = collecting();
    options.on_duplicate_key(deserialize_options::duplicate_key_action::exception);

    formats fmts = formats_builder().register_container<std::vector<value>>().compose_checked(formats::defaults());

    deserialization_context cxt(fmts, std::nullopt, jsonv::path(), nullptr, options);
    auto               rdr = open(R"([ { "a": 1, "a": 2 }, 5, { "b": 1, "b": 2 } ])");

    ensure(!cxt.deserialize<std::vector<value>>(rdr).has_value());
    ensure_eq(2U, cxt.problems().size());
    ensure_eq(path::create("[0].a"), cxt.problems().at(0).path());
    ensure_eq(path::create("[2].b"), cxt.problems().at(1).path());
}

namespace
{

constexpr auto replace = deserialize_options::duplicate_key_action::replace;
constexpr auto ignore  = deserialize_options::duplicate_key_action::ignore;

/// A context to peek for, settling a repeated key by \a on_duplicate.
deserialization_context peeking(deserialize_options::duplicate_key_action on_duplicate)
{
    return deserialization_context(formats::defaults(),
                                   std::nullopt,
                                   jsonv::path(),
                                   nullptr,
                                   deserialize_options::create_default().on_duplicate_key(on_duplicate)
                                  );
}

}

TEST(deserialize_peek_value_leaves_the_reader_where_it_was)
{
    // Every kind of source, including the two which own what they read: the second cursor borrows that rather than
    // copying it, and has to read the same thing the first would.
    const std::string_view text = R"([ { "a": [ 1, 2 ], "b": "c" }, 99 ])";
    const value            tree = parse(text);

    std::vector<std::function<reader ()>> sources =
        {
            [&] { return reader(text); },
            [&] { return reader(std::string(text)); },
            [&] { return reader::from_value(tree); },
            [&] { return reader::from_value(value(tree)); },
        };

    for (const auto& make : sources)
    {
        reader rdr = make();
        (void) rdr.next_token();   // onto the `[`
        (void) rdr.next_token();   // onto the `{`

        ensure_eq(tree.at(0), detail::peek_value(peeking(replace), rdr));
        ensure_eq(tree.at(0), detail::peek_value(peeking(replace), rdr));
        ensure(rdr.current_type() == ast_node_type::object_begin);

        // The reader itself goes on to read it all the same.
        ensure_eq(tree.at(0), read_value(rdr));
        ensure_eq(99, rdr.current().as<ast_node::integer>().value());
    }
}

TEST(deserialize_peek_value_reads_a_scalar_where_it_sits)
{
    // A scalar is one token, so a reader over text reads it without opening a second cursor -- and what it reads has to
    // be what reading it for real would, an escaped string and integers past 64 bits included.
    for (std::string_view text : { R"("plain")",
                                   R"("esc\u0061ped")",
                                   "true",
                                   "false",
                                   "null",
                                   "-12",
                                   "4.5",
                                   "18446744073709551615",
                                   "18446744073709551616",
                                 }
        )
    {
        reader              rdr    = open(text);
        const ast_node_type type   = rdr.current_type();
        const value         peeked = detail::peek_value(peeking(replace), rdr);

        ensure(rdr.current_type() == type);
        ensure_eq(read_value(rdr), peeked);
    }

    // It refuses what reading it for real refuses, and leaves the reader on the value it refused.
    {
        reader rdr = open("1e400");
        ensure_throws(std::invalid_argument, detail::peek_value(peeking(replace), rdr));
        ensure(rdr.current_type() == ast_node_type::decimal);
    }
    {
        reader rdr = open(R"("\uD800")");
        ensure_throws(parse_error, detail::peek_value(peeking(replace), rdr));
        ensure(rdr.current_type() == ast_node_type::string_escaped);
    }

#if JSONV_TEST_COUNTS_ALLOCATIONS
    // And one which is not a string costs nothing to peek at, which is what an `enum_adapter` mapping numbers pays.
    for (std::string_view text : { "true", "null", "-12", "4.5" })
    {
        reader                   rdr = open(text);
        const deserialization_context cxt = peeking(replace);

        allocation_counter allocations;
        const value        peeked = detail::peek_value(cxt, rdr);
        const std::size_t  cost   = allocations.count();

        ensure_eq(0U, cost);
        ensure_eq(parse(text), peeked);
    }
#endif
}

TEST(deserialize_peek_part_way_through_a_document)
{
    // A reader over a tree is a stack of frames as well as a position, and the second cursor needs all of it: the path
    // to here, and where each enclosing structure goes next.
    const std::string_view text = R"({ "x": [ 1, { "y": [ 2, 3 ] } ], "z": 4 })";
    const value            tree = parse(text);

    for (bool from_value : { false, true })
    {
        reader rdr = from_value ? reader::from_value(tree) : reader(text);
        (void) rdr.next_token();   // onto the `{`
        (void) rdr.next_token();   // onto "x"
        (void) rdr.next_token();   // onto the `[`
        (void) rdr.next_token();   // onto 1
        (void) rdr.next_token();   // onto the inner `{`
        (void) rdr.next_token();   // onto "y"
        (void) rdr.next_token();   // onto the inner `[`

        ensure_eq(parse("[ 2, 3 ]"), detail::peek_value(peeking(replace), rdr));
        ensure_eq(path::create(".x[1].y"), rdr.current_path());

        ensure_eq(parse("[ 2, 3 ]"), read_value(rdr));
        ensure(rdr.current_type() == ast_node_type::object_end);
        (void) rdr.next_token();
        ensure(rdr.current_type() == ast_node_type::array_end);
        (void) rdr.next_token();
        ensure_eq(std::string("z"), rdr.current().visit_key([] (const auto& k) { return std::string(k.value()); }));
    }
}

TEST(deserialize_peek_members_reads_only_what_was_asked_for)
{
    // Each `1e400` is something `read_value` refuses, so reaching any of them would throw. An escaped key is compared
    // decoded, as `parse` would have it.
    const std::string_view text = R"({ "blob": [ 1e400 ], "k": 1, "skip": 1e400, "\u006b": 2, "other": "x" })";
    const std::set<std::string, std::less<>> keys = { "k", "other", "absent" };

    reader rdr(text);
    (void) rdr.next_token();   // onto the `{`

    ensure_eq(parse(R"({ "k": 2, "other": "x" })"), detail::peek_members(peeking(replace), rdr, keys));
    ensure(rdr.current_type() == ast_node_type::object_begin);

    // Something which is not an object has no members to find.
    for (std::string_view other : { "[ 1 ]", "5", "null" })
    {
        auto scalar = open(other);
        ensure_eq(value(), detail::peek_members(peeking(replace), scalar, keys));
    }

    // A document which stops part-way through gives back what it had.
    for (std::string_view truncated : { R"({ "k": 1, "blob": [ 1, 2)", R"({ "k": 1, "other":)" })
    {
        auto partial = open(truncated);
        ensure_eq(parse(R"({ "k": 1 })"), detail::peek_members(peeking(replace), partial, keys));
    }
}

TEST(deserialize_peek_keeps_the_repeated_key_the_reader_would)
{
    // A peek settles a repeated key as `read_value` settles one for a deserializer under the same option: `ignore`
    // keeps the first, `replace` the last, and `exception` refuses. The repeat which is not kept is stepped over, so a
    // `1e400` in it is never read. `peek_members` only looks at the keys it was asked for, so a repeat of any other key
    // goes unremarked.
    const std::string_view text = R"({ "k": { "n": 1, "n": 2 }, "k": [ 1e400 ] })";
    reader                 rdr(text);
    (void) rdr.next_token();   // onto the `{`

    ensure_eq(parse(R"({ "k": { "n": 1 } })"), detail::peek_members(peeking(ignore), rdr, { "k" }));
    ensure_eq(parse(R"({ "k": { "n": 1 } })"), detail::peek_value(peeking(ignore), rdr));
    ensure_throws(std::invalid_argument, detail::peek_members(peeking(replace), rdr, { "k" }));

    const std::string_view last = R"({ "k": 1, "k": 2, "j": { "n": 1, "n": 2 } })";
    reader                 last_rdr(last);
    (void) last_rdr.next_token();   // onto the `{`

    ensure_eq(parse(R"({ "k": 2 })"), detail::peek_members(peeking(replace), last_rdr, { "k" }));
    ensure_eq(parse(R"({ "k": 2, "j": { "n": 2 } })"), detail::peek_value(peeking(replace), last_rdr));

    constexpr auto exception = deserialize_options::duplicate_key_action::exception;
    ensure_throws(deserialization_error, detail::peek_members(peeking(exception), last_rdr, { "k" }));
    ensure_throws(deserialization_error, detail::peek_members(peeking(exception), last_rdr, { "j" }));
    ensure_throws(deserialization_error, detail::peek_value(peeking(exception), last_rdr));

    const std::string_view elsewhere = R"({ "k": 1, "j": 2, "j": 3 })";
    reader                 elsewhere_rdr(elsewhere);
    (void) elsewhere_rdr.next_token();   // onto the `{`
    ensure_eq(parse(R"({ "k": 1 })"), detail::peek_members(peeking(exception), elsewhere_rdr, { "k" }));
}

TEST(deserialize_peek_repeated_key_is_placed_where_the_context_says)
{
    // A refusal is placed as `read_value` places one: under the context's own location where it has one, followed by
    // the part of the reader's path below where the peek began. With none, the reader's path is the answer.
    const std::string_view text = R"({ "k": 1, "j": { "n": 1, "n": 2 }, "k": 2 })";
    reader                 rdr(text);
    (void) rdr.next_token();   // onto the `{`

    auto refused_at = [&rdr] (const jsonv::path& base, auto peek) -> jsonv::path
                      {
                          deserialization_context cxt(formats::defaults(),
                                                      std::nullopt,
                                                      base,
                                                      nullptr,
                                                      deserialize_options::create_default().on_duplicate_key(
                                                          deserialize_options::duplicate_key_action::exception
                                                      )
                                                     );
                          try
                          {
                              (void) peek(cxt);
                          }
                          catch (const deserialization_error& ex)
                          {
                              return ex.path();
                          }
                          ensure(false);
                          return jsonv::path();
                      };

    auto members = [&rdr] (const deserialization_context& cxt) { return detail::peek_members(cxt, rdr, { "k" }); };
    auto nested  = [&rdr] (const deserialization_context& cxt) { return detail::peek_members(cxt, rdr, { "j" }); };
    auto whole   = [&rdr] (const deserialization_context& cxt) { return detail::peek_value(cxt, rdr); };

    ensure_eq(path::create(".payload.k"),   refused_at(path::create(".payload"), members));
    ensure_eq(path::create(".payload.j.n"), refused_at(path::create(".payload"), nested));
    ensure_eq(path::create(".payload.j.n"), refused_at(path::create(".payload"), whole));
    ensure_eq(path::create(".k"),           refused_at(jsonv::path(),            members));
    ensure_eq(path::create(".j.n"),         refused_at(jsonv::path(),            whole));
}

TEST(deserialize_peek_members_jumps_over_what_it_does_not_want)
{
    // A tape records where each structure ends, so stepping over one costs the same however large it is.
    // `reader::next_key` walks every token of the value instead, which here is two orders of magnitude apart -- far
    // enough that a generous absolute bound tells them apart without being sensitive to the machine.
    const int elements = JSONV_DEBUG ? 20000 : 200000;
    const int peeks    = 2000;

    std::string document = R"({ "blob": [ )";
    for (int idx = 0; idx < elements; ++idx)
        document += "[ 1 ], ";
    document += R"(0 ], "kind": "x" })";

    reader rdr(document);
    (void) rdr.next_token();   // onto the `{`

    const std::set<std::string, std::less<>> keys = { "kind" };
    const deserialization_context            cxt  = peeking(replace);

    auto started = std::chrono::steady_clock::now();

    std::size_t found = 0U;
    for (int idx = 0; idx < peeks; ++idx)
        found += detail::peek_members(cxt, rdr, keys).size();

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - started
                   ).count();

    ensure_eq(std::size_t(peeks), found);
    ensure_lt(elapsed, 5000);
}

namespace
{

struct nested_failure
{
    std::string  first;
    std::int64_t second;
};

/// Reports a problem *after* a successful sub-deserialization, so ASan can say whether the first one leaked.
class nested_failure_adapter final :
        public adapter_for<nested_failure>
{
protected:
    std::expected<nested_failure, ast_node_type>
    create(deserialization_context& context, reader& from) const override
    {
        if (auto opened = context.expect(from, ast_node_type::array_begin); !opened)
            return std::unexpected(opened.error());

        (void) from.next_token();

        nested_failure out;
        if (auto first = context.deserialize<std::string>(from))
            out.first = std::move(*first);
        else
            return std::unexpected(first.error());

        return context.problem(context.path(), "Second element is unacceptable");
    }

    void serialize(const serialization_context&, const nested_failure&, writer& to) const override
    {
        to.array_begin().array_end();
    }
};

}

TEST(deserialize_no_leak_when_a_later_step_fails)
{
    static nested_failure_adapter instance;

    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_adapter(&instance);

    deserialization_context cxt(fmts);
    auto               rdr = open(R"([ "a string long enough to need the heap rather than the small-string buffer", 2 ])");

    auto result = cxt.deserialize<nested_failure>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
}

TEST(deserialize_problems_survive_the_value_bridge)
{
    // `unassociated` has no deserializer, so the failure happens below a value-based adapter and has to come back up
    // through the catch boundary in deserialization_context::deserialize with its path and cause intact.
    struct unassociated { };

    deserialization_context cxt(formats::defaults());
    value              val = parse(R"({ "o": { "i": 5 } })");

    try
    {
        deserialization_context::path_scope scope(cxt, "o");
        (void) cxt.deserialize<unassociated>(val.at("o"));
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure_eq(path::create(".o"), err.problems().at(0).path());
        ensure(err.problems().at(0).nested_ptr());
        ensure_throws(no_deserializer, (std::rethrow_exception(err.problems().at(0).nested_ptr()), 0));
    }

    // The problems went into the exception rather than being left behind to be reported a second time.
    ensure(cxt.problems().empty());
}

TEST(deserialize_borrowed_string_view_points_at_the_source)
{
    // The built-in `std::string_view` adapter returns a view of what it was handed. Running deserialization through a
    // reader must not turn that into a view of a temporary the bridge materialised and then destroyed.
    value source = std::string("long enough to be on the heap rather than in the small-string buffer");

    auto borrowed = deserialize<std::string_view>(source);
    ensure_eq(source.as_string_view(), borrowed);
    ensure(borrowed.data() == source.as_string_view().data());
}

TEST(deserialize_borrowed_string_view_survives_a_nested_adapter)
{
    // The same has to hold when the view comes back up through a bridged adapter rather than directly.
    formats fmts = formats::compose({ formats_builder()
                                          .register_container<std::vector<std::string_view>>(),
                                      formats::defaults()
                                    });

    value source = parse(R"([ "the first long string value here", "the second long string value here" ])");
    auto  views  = deserialize<std::vector<std::string_view>>(source, fmts);

    ensure_eq(2U, views.size());
    for (std::size_t idx = 0; idx < views.size(); ++idx)
        ensure(views.at(idx).data() == source.at(idx).as_string_view().data());
}

TEST(deserialize_value_backed_reader_lends_its_subtree)
{
    value  source = parse(R"({ "a": [ 1, 2 ], "b": 3 })");
    reader rdr    = reader::from_value(source);

    (void) rdr.next_token();   // onto {
    ensure(&rdr.current_value().value() == &source);

    (void) rdr.next_token();   // onto the key "a" -- half a member, not a value
    ensure(!rdr.current_value());

    (void) rdr.next_token();   // onto [
    ensure(&rdr.current_value().value() == &source.at("a"));

    // A text-backed reader has no tree to lend.
    auto text = open(R"({ "a": 1 })");
    ensure(!text.current_value());
}

TEST(deserialize_read_value_duplicate_keys_match_parse)
{
    // `parse` uses duplicate_key_action::replace, so the last one wins. Reading the same text through a reader has to
    // select the same member, or deserializing from text and from the parsed tree disagree.
    for (const auto* source : { R"({"x":1,"x":2})",
                                R"({"x":1,"x":2,"x":3})",
                                R"({"a":0,"x":1,"x":2,"b":9})"
                              })
    {
        auto rdr = open(source);
        ensure_eq(parse(source), read_value(rdr));
    }
}

TEST(deserialize_bridged_failure_names_the_element_it_started_on)
{
    // The bridge consumes its subtree before the older body runs, so a conversion failure must still be reported at
    // the value the deserialization started on rather than at the sibling the cursor has moved to.
    deserialization_context cxt(formats::defaults());
    auto               rdr = open(R"([ "wrong", 99 ])");

    (void) rdr.next_token();   // onto "wrong", element [0]

    auto result = cxt.deserialize<std::int64_t>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
}

TEST(deserialize_error_message_separates_an_empty_path_from_the_message)
{
    try
    {
        auto rdr = open("}");
        (void) read_value(rdr);
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure(err.path().empty());
        ensure_eq(std::string("Deserialization error: Unexpected token of type end of object while reading a value"),
                  std::string(err.what())
                 );
    }
}

TEST(deserialize_error_with_no_problems_synthesises_one)
{
    // `problems()` documents that the list is never empty, and unlike `path()` and `nested_ptr()` it has nothing
    // sensible to fall back on -- so the invariant is established when the error is built rather than guarded at
    // each accessor. A caller reading `problems()` and a caller reading `what()` should not disagree about whether
    // anything went wrong.
    deserialization_error err{ deserialization_error::problem_list() };

    ensure_eq(1U, err.problems().size());
    ensure(err.problems().at(0).path().empty());
    ensure(!err.problems().at(0).nested_ptr());
    ensure(!std::string(err.what()).empty());
}

namespace
{

/// A context reporting its problems in `config.json`. Every argument before the name is spelled out, since a string in
/// the place of the user data would be taken for the user data.
deserialization_context named_context(const formats& fmts, const deserialize_options& options = deserialize_options())
{
    return deserialization_context(fmts, std::nullopt, jsonv::path(), nullptr, options, "config.json");
}

/// What \a run throws.
template <typename FRun>
deserialization_error thrown_by(const FRun& run)
{
    try
    {
        run();
    }
    catch (const deserialization_error& err)
    {
        return err;
    }

    ensure(!"deserialization_error was not thrown");
    return deserialization_error(deserialization_error::problem_list());
}

/// Read from a second document named `other.json`, as an adapter resolving an include from another file would.
struct included
{
    std::int64_t value;
};

formats included_formats()
{
    static auto instance = make_deserializer([] (deserialization_context& context, reader& from) -> included
                                             {
                                                 (void) from.next_value();

                                                 deserialization_context other(context.formats(),
                                                                               std::nullopt,
                                                                               jsonv::path(),
                                                                               nullptr,
                                                                               deserialize_options(),
                                                                               "other.json"
                                                                              );
                                                 auto inner = deserialize<std::int64_t>(R"("not a number")", other);
                                                 return included{ inner };
                                             });

    formats out = formats::compose({ formats::defaults() });
    out.register_deserializer(&instance);
    return out;
}

/// Fails without recording why, which a deserializer is not meant to do and nothing stops.
struct silent_failure
{ };

formats silent_failure_formats()
{
    static auto instance = make_deserializer([] (reader&) -> std::expected<silent_failure, ast_node_type>
                                             {
                                                 return std::unexpected(ast_node_type::error);
                                             });

    formats out = formats::compose({ formats::defaults() });
    out.register_deserializer(&instance);
    return out;
}

}

TEST(deserialize_error_names_the_source_and_the_path_within_it)
{
    deserialization_context cxt = named_context(triple_formats());
    auto err = thrown_by([&] { (void) deserialize<triple>(R"({ "a": 1, "b": "two", "c": 3 })", cxt); });

    ensure_eq(std::string("Deserialization error at config.json#.b: Read node of type string when expecting integer"),
              std::string(err.what())
             );
    ensure_eq(path::create(".b"), err.path());
    ensure_eq(std::string("config.json"), err.source_name());
    ensure_eq(std::string("config.json"), err.problems().at(0).source_name());
}

TEST(deserialize_error_without_a_source_name_reads_as_it_did)
{
    deserialization_context cxt(triple_formats());
    auto err = thrown_by([&] { (void) deserialize<triple>(R"({ "a": 1, "b": "two", "c": 3 })", cxt); });

    ensure_eq(std::string("Deserialization error at .b: Read node of type string when expecting integer"),
              std::string(err.what())
             );
    ensure(err.source_name().empty());
}

TEST(deserialize_error_names_the_source_of_a_parse_failure)
{
    // The parse fails before there is any position in the document to name, so the document is named alone -- an
    // empty path is no position rather than the root, and `config.json#.` would say otherwise.
    deserialization_context cxt = named_context(formats::defaults());
    auto err = thrown_by([&] { (void) deserialize<std::int64_t>("[ 1, ", cxt); });

    ensure(std::string_view(err.what()).starts_with("Deserialization error at config.json: Could not parse JSON: "));
    ensure_eq(std::string("config.json"), err.source_name());
}

TEST(deserialize_error_names_the_source_of_every_problem_collected)
{
    deserialization_context cxt = named_context(triple_formats(), collecting());
    auto err = thrown_by([&]
                         {
                             (void) deserialize<std::vector<triple>>(
                                 R"([ { "a": "x", "b": 2, "c": 3 }, { "a": 1, "b": 2, "c": "z" } ])",
                                 cxt
                             );
                         });

    ensure_eq(2U, err.problems().size());
    for (const auto& problem : err.problems())
        ensure_eq(std::string("config.json"), problem.source_name());

    const std::string what = err.what();
    ensure(what.starts_with("2 deserialization errors:"));
    ensure(what.find("\n - at config.json#[0].a: ") != std::string::npos);
    ensure(what.find("\n - at config.json#[1].c: ") != std::string::npos);
}

TEST(deserialize_error_names_the_source_under_a_base_path)
{
    deserialization_context cxt(triple_formats(),
                                std::nullopt,
                                path::create(".root"),
                                nullptr,
                                deserialize_options(),
                                "config.json"
                               );
    auto err = thrown_by([&] { (void) deserialize<triple>(R"({ "a": 1, "b": "two", "c": 3 })", cxt); });

    ensure_eq(path::create(".root.b"), err.path());
    ensure(std::string_view(err.what()).starts_with("Deserialization error at config.json#.root.b: "));
}

TEST(deserialize_error_names_the_source_through_the_value_bridge)
{
    // An adapter on the bridge reports failure by throwing, and what it throws is folded onto the context -- which is
    // where it is named, whichever source the bridge was reading.
    const std::string text = R"({ "a": 1, "b": "two", "c": 3 })";
    {
        deserialization_context cxt = named_context(bridged_formats());
        auto err = thrown_by([&] { (void) cxt.deserialize<bridged_triple>(parse(text)); });
        ensure_eq(std::string("config.json"), err.source_name());
        ensure(std::string_view(err.what()).starts_with("Deserialization error at config.json"));
    }
    {
        deserialization_context cxt = named_context(bridged_formats());
        auto err = thrown_by([&] { (void) deserialize<bridged_triple>(text, cxt); });
        ensure_eq(std::string("config.json"), err.source_name());
        ensure(std::string_view(err.what()).starts_with("Deserialization error at config.json"));
    }
}

TEST(deserialize_error_keeps_the_source_a_problem_was_named_in)
{
    // A deserializer reading a second document reports that document's problems under its own name, and folding them
    // onto this context does not rename them.
    deserialization_context cxt = named_context(included_formats());
    auto err = thrown_by([&] { (void) deserialize<included>("{}", cxt); });

    ensure_eq(std::string("other.json"), err.source_name());
    ensure_eq(std::string("Deserialization error at other.json: Read node of type string when expecting integer"),
              std::string(err.what())
             );
}

TEST(deserialize_error_names_the_source_of_a_positioned_failure)
{
    // A positioned deserialization throws nothing, so the name has to be on the problem by the time it is recorded.
    deserialization_context cxt = named_context(formats::defaults());
    auto               rdr = open(R"("five")");

    ensure(!cxt.deserialize<std::int64_t>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("config.json"), cxt.problems().at(0).source_name());
}

TEST(deserialize_error_names_the_source_of_a_failure_nobody_described)
{
    {
        deserialization_context cxt = named_context(silent_failure_formats());
        auto err = thrown_by([&] { (void) deserialize<silent_failure>("{}", cxt); });
        ensure_eq(std::string("Deserialization error at config.json: Unspecified deserialization error"),
                  std::string(err.what())
                 );
    }
    {
        deserialization_context cxt(silent_failure_formats());
        auto err = thrown_by([&] { (void) deserialize<silent_failure>("{}", cxt); });
        ensure_eq(std::string("Deserialization error: Unspecified deserialization error"), std::string(err.what()));
    }
}

TEST(deserialize_read_value_rejects_an_unmatched_close)
{
    // These reach `read_value` only from a tape which was never validated, and the diagnostic it builds asks the
    // reader where it is -- which used to walk off the end of the path builder's element stack.
    for (const auto* source : { "}", "]" })
    {
        auto rdr = open(source);
        ensure_throws(deserialization_error, read_value(rdr));
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

TEST(deserialize_legacy_dispatch_passes_a_const_value)
{
    // `invoke_deserialize` checks `invocable<F, const value&>` before choosing this shape, so it has to invoke with a
    // `const value&` too -- handing over the mutable local it materialised would run an overload the check never
    // tested, and can fail to compile when the two return different types.
    static auto instance = make_deserializer(constness_probe());

    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_deserializer(&instance, duplicate_type_action::replace);

    ensure_eq(std::string("const"), deserialize<std::string>(value("ignored"), fmts));
}

TEST(deserialize_string_view_from_text_points_at_the_source)
{
    // A canonical string token *is* the string, so a view of it is a view of the document the caller handed over --
    // no decoding, no copy, and valid for exactly as long as that source is.
    std::string        source = R"("long enough to be on the heap rather than in the small-string buffer")";
    deserialization_context cxt(formats::defaults());
    reader             rdr(std::string_view{ source });
    (void) rdr.next_token();

    auto borrowed = cxt.deserialize<std::string_view>(rdr);
    ensure(borrowed.has_value());
    ensure_eq(std::string_view("long enough to be on the heap rather than in the small-string buffer"), *borrowed);
    ensure(borrowed->data() == source.data() + 1);
    ensure(cxt.problems().empty());
}

TEST(deserialize_string_view_from_an_escaped_string_is_refused)
{
    // An escaped string has no decoded form anywhere in the source to point at, so there is nothing to borrow. A
    // silent copy into storage which dies with the call is the thing worth refusing.
    deserialization_context cxt(formats::defaults());
    auto               rdr = open(R"("a \u00e9 which the source spelt the long way")");

    auto result = cxt.deserialize<std::string_view>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure(cxt.problems().at(0).message().find("std::string_view") != std::string::npos);

    // `std::string` is the documented way to get the same text out, and decodes it.
    auto               other = open(R"("a \u00e9 which the source spelt the long way")");
    deserialization_context decoding(formats::defaults());
    auto               decoded = decoding.deserialize<std::string>(other);
    ensure(decoded.has_value());
    // Explicit UTF-8 bytes rather than `\u00e9`: a universal character name in a narrow literal is encoded in the
    // compiler's execution character set, which is not UTF-8 everywhere, while the decoder always produces UTF-8.
    ensure_eq(std::string("a \xc3\xa9 which the source spelt the long way"), *decoded);   // U+00E9
}

TEST(deserialize_nested_string_view_from_text_points_at_the_source)
{
    // This was a refusal while `container_adapter` was on the bridge: the container materialised the whole array and
    // every element then borrowed from a temporary which died with the deserialization. Walking the reader means there
    // is no temporary, so each element views the source document exactly as a lone `std::string_view` does -- and is
    // valid for exactly as long as that source is.
    formats fmts = formats::compose({ formats_builder()
                                          .register_container<std::vector<std::string_view>>(),
                                      formats::defaults()
                                    });

    std::string        source = R"([ "the first long string value here", "the second long string value here" ])";
    deserialization_context cxt(fmts);
    reader             rdr(std::string_view{ source });
    (void) rdr.next_token();

    auto views = cxt.deserialize<std::vector<std::string_view>>(rdr);
    ensure(views.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(2U, views->size());
    ensure_eq(std::string_view("the first long string value here"),  views->at(0));
    ensure_eq(std::string_view("the second long string value here"), views->at(1));

    // Views of the document itself, not copies of it.
    for (const auto& view : *views)
        ensure(view.data() >= source.data() && view.data() < source.data() + source.size());
}

TEST(deserialize_string_view_under_a_bridged_composite_from_text_is_refused)
{
    // The coverage the tests above gave up. A composite which still materialises is exactly where a view of the
    // materialised tree would dangle, and `source_is_temporary` is what a deserializer asks to notice it. Nothing in
    // the library creates that situation any more, so the composite is written against the `value` interface directly,
    // as an adapter outside it still may be.
    class view_holder_adapter final :
            public value_adapter_for<view_holder>
    {
    protected:
        view_holder create(deserialization_context& context, const value& from) const override
        {
            return view_holder{ context.deserialize<std::string_view>(from.at("s")) };
        }

        value to_json(const serialization_context&, const view_holder&) const override
        {
            return object();
        }
    };

    static view_holder_adapter instance;

    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_adapter(&instance, duplicate_type_action::replace);

    deserialization_context cxt(fmts);
    auto               rdr = open(R"({ "s": "a long string value which would be left dangling" })");

    ensure(!cxt.deserialize<view_holder>(rdr).has_value());
    ensure(!cxt.problems().empty());
    ensure(cxt.problems().at(0).message().find("std::string_view") != std::string::npos);
}

TEST(deserialize_dsl_member_string_view_from_text_points_at_the_source)
{
    // The other half of the same change. A DSL-described type materialised its whole subtree, so a `std::string_view`
    // member had to be refused -- for a structural reason rather than a semantic one. With nothing materialised the
    // member views the document exactly as a lone `std::string_view` does.
    formats fmts = formats_builder()
                       .type<view_holder>()
                           .member("s", &view_holder::s)
                   .compose_checked(formats::defaults());

    std::string        source = R"({ "s": "a long string value which is not copied anywhere" })";
    deserialization_context cxt(fmts);
    reader             rdr(std::string_view{ source });
    (void) rdr.next_token();

    auto held = cxt.deserialize<view_holder>(rdr);
    ensure(held.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(std::string_view("a long string value which is not copied anywhere"), held->s);

    // A view of the document itself, not a copy of it.
    ensure(held->s.data() >= source.data() && held->s.data() < source.data() + source.size());
}

TEST(deserialize_scope_free_loop_over_text_stays_linear)
{
    // Naming the position a failure started at by building a path before every deserialization is quadratic here: a
    // text-backed `current_path` rescans from the start of the document, so element `i` costs `i`. Leaving the cursor
    // on the value until the deserialization succeeds gets the same answer for nothing.
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

    deserialization_context cxt(formats::defaults());
    jsonv::reader      rdr(document);
    (void) rdr.next_token();   // onto [
    (void) rdr.next_token();   // onto the first element

    std::int64_t total = 0;
    for (int idx = 0; idx < elements; ++idx)
    {
        auto element = cxt.deserialize<std::int64_t>(rdr);
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

TEST(deserialize_bridged_structure_failure_names_the_enclosing_structure)
{
    // A structure has to be walked to be read, so unlike a scalar the cursor is past it before the older body runs.
    // The sibling it lands on is a perfectly valid value and naming it would send the reader of the error to the
    // wrong place; the structure around the failure is still true of it.
    {
        deserialization_context cxt(bridged_formats());
        auto               rdr = open(R"({ "a": [ [], 99 ] })");

        while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
            (void) rdr.next_token();
        (void) rdr.next_token();   // onto the inner [], which is `.a[0]`

        auto result = cxt.deserialize<bridged_int>(rdr);
        ensure(!result.has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_ne(path::create(".a[1]"), cxt.problems().at(0).path());
        ensure_eq(path::create(".a"), cxt.problems().at(0).path());
    }

    // Landing on a closing token is the case where `current_path` already names the structure, so nothing is dropped.
    {
        deserialization_context cxt(bridged_formats());
        auto               rdr = open(R"({ "a": [ [] ] })");

        while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
            (void) rdr.next_token();
        (void) rdr.next_token();   // onto the inner []

        ensure(!cxt.deserialize<bridged_int>(rdr).has_value());
        ensure_eq(path::create(".a"), cxt.problems().at(0).path());
    }
}

TEST(deserialize_native_structure_failure_names_the_value_it_was_on)
{
    // The other half of the case above: a deserializer which reads the reader never moves off the value it rejected,
    // so the enclosing structure is not the best it can say. `.a[0]` is where the array actually is.
    deserialization_context cxt(formats::defaults());
    auto               rdr = open(R"({ "a": [ [], 99 ] })");

    while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
        (void) rdr.next_token();
    (void) rdr.next_token();   // onto the inner [], which is `.a[0]`

    auto result = cxt.deserialize<std::int64_t>(rdr);
    ensure(!result.has_value());
    ensure(result.error() == ast_node_type::array_begin);
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[0]"), cxt.problems().at(0).path());
}

TEST(deserialize_stale_cursor_does_not_leak_into_a_nested_reader)
{
    // The bridge marks the reader it walked past, not a global flag: a nested deserialization running on its own
    // value-backed reader must still get that reader's exact position. The DSL is the composite which still
    // materialises -- `optional_adapter` used to stand in here and now reads the reader, so it no longer would.
    deserialization_context cxt(triple_formats());
    auto               rdr = open(R"([ { "a": "wrong", "b": 2, "c": 3 }, 99 ])");
    (void) rdr.next_token();   // onto the object

    // The object is materialised (cursor moves past it); the member deserialization below runs on a fresh value reader.
    ensure(!cxt.deserialize<triple>(rdr).has_value());
    ensure(!cxt.problems().empty());
    ensure_eq(path::create(".a"), cxt.problems().at(0).path());
}

TEST(deserialize_malformed_key_does_not_escape_the_diagnostic)
{
    // Naming where a failure happened walks the object keys between here and the start of the document, and decoding
    // one of those can itself throw -- an unpaired surrogate does. That happens on the unwind, inside a `noexcept`
    // destructor, so getting it wrong terminates the process rather than reporting anything.
    deserialization_context cxt(formats::defaults());
    auto               rdr = open(R"({ "a": [], "\uD800": 0 })");

    while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
        (void) rdr.next_token();

    auto result = cxt.deserialize<std::int64_t>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());

    // The message describes the deserialization that failed, not the parse error hit while looking for a name for it.
    ensure(cxt.problems().at(0).message().find("surrogate") == std::string::npos);
}

TEST(deserialize_explicit_location_outranks_the_reader)
{
    // A scope, and a base path, say where the *deserializer* is. Both are authoritative over anything worked out from
    // where the reader happens to be sitting.
    {
        deserialization_context cxt(formats::defaults(), std::nullopt, path::create(".fragment"));
        auto               rdr = open("[]");

        ensure(!cxt.deserialize<std::int64_t>(rdr).has_value());
        ensure_eq(path::create(".fragment"), cxt.problems().at(0).path());
    }

    {
        deserialization_context             cxt(formats::defaults());
        auto                                rdr = open("[]");
        deserialization_context::path_scope scope(cxt, "renamed");

        ensure(!cxt.deserialize<std::int64_t>(rdr).has_value());
        ensure_eq(path::create(".renamed"), cxt.problems().at(0).path());
    }
}

TEST(deserialize_failure_location_does_not_outlive_its_deserialization)
{
    // The `deserialization_error` branch folds problems which already carry their own paths, so it never asks where it
    // is
    // -- and must not leave the location the bridge deposited lying around for the next failure to pick up. What this
    // needs is a composite which both materialises and reports by throwing; `optional_adapter` stood in here until it
    // read the reader directly, and the DSL until it did, so the adapter is written against the `value` interface
    // here rather than borrowed from whichever one has not been ported yet.
    deserialization_context cxt(bridged_formats());
    auto               rdr = open(R"([ { "a": 1, "b": 2, "c": "x" }, "not a number" ])");
    (void) rdr.next_token();   // onto the object, which is `[0]`

    // The object is materialised, so the cursor moves past it; the nested deserialization then throws a
    // `deserialization_error`, which is folded rather than re-located.
    ensure(!cxt.deserialize<bridged_triple>(rdr).has_value());
    auto after_first = cxt.problems().size();
    ensure_gt(after_first, 0U);

    // The cursor is now on `[1]`, and that is where this failure is.
    ensure(!cxt.deserialize<std::int64_t>(rdr).has_value());
    ensure_gt(cxt.problems().size(), after_first);
    ensure_eq(path::create("[1]"), cxt.problems().back().path());
}

TEST(deserialize_expect_is_independent_of_a_pending_failure_location)
{
    // `expect` is about where the reader is sitting, which is always right at the moment it is asked. The location a
    // bridge leaves behind on its way out of a failure is for translating that one exception and nothing else.
    deserialization_context cxt(bridged_formats());
    auto               rdr = open(R"({ "a": [ [], 99 ] })");

    while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
        (void) rdr.next_token();   // onto the `[` of `.a`
    (void) rdr.next_token();       // onto the inner `[]`, which is `.a[0]`

    // Reaching the deserializer through `formats` is public, discouraged, and -- the point here -- skips the handler
    // which would have translated and consumed what the bridge leaves behind.
    alignas(bridged_int) std::byte place[sizeof(bridged_int)];
    try
    {
        (void) cxt.formats().deserialize(typeid(bridged_int), rdr, static_cast<void*>(place), cxt);
        ensure(!"deserializing an integer from an array did not fail");
    }
    catch (const std::exception&)
    { }

    ensure(cxt.problems().empty());

    // The reader is on `99` now. That is where this mismatch is, regardless of what the abandoned deserialization left.
    ensure(!cxt.expect(rdr, ast_node_type::string_canonical).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[1]"), cxt.problems().at(0).path());
}

TEST(deserialize_value_and_text_sources_agree)
{
    std::string source = R"({ "i": 5, "a": [ 1, 2, 3 ], "s": "thing" })";

    ensure_eq(deserialize<std::int64_t>(parse(source).at_path(".i")), 5);

    deserialization_context cxt(formats::defaults());
    auto               rdr = open(source);
    ensure_eq(parse(source), *cxt.deserialize<value>(rdr));
}

TEST(deserialize_value_and_text_sources_agree_beyond_64_bits)
{
    // A reader over text builds a `value` from an integer token without going through `parse`, so it has to make the
    // same choice for a literal no 64-bit integer holds (#206).
    const std::string source = "[18446744073709551615, 18446744073709551616, -99999999999999999999999]";

    const value deserialized = deserialize<value>(source);
    ensure_eq(parse(source), deserialized);
    ensure(deserialized.at(0).kind() == kind::integer);
    ensure(deserialized.at(1).kind() == kind::decimal);
    ensure(deserialized.at(2).kind() == kind::decimal);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// deserialize_options::on_error and max_failures                                                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(deserialize_collect_default_stops_at_the_first_problem)
{
    // The default is `fail_immediately` and must stay that way: every existing caller gets one problem and a stop.
    value source = parse(R"({ "a": "x", "b": "y", "c": "z" })");

    try
    {
        (void) deserialize<triple>(source, triple_formats());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure_eq(path::create(".a"), err.problems().at(0).path());
    }
}

TEST(deserialize_collect_all_gathers_every_bad_member)
{
    value source = parse(R"({ "a": "x", "b": 2, "c": "z" })");

    try
    {
        (void) deserialize<triple>(source, triple_formats(), collecting());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(2U, err.problems().size());
        ensure_eq(std::string(".a .c"), problem_paths(err));
    }
}

TEST(deserialize_collect_all_gathers_every_missing_member)
{
    // `mutate` throws for a missing required field as well as for a failed conversion, so recovery reports all of
    // them rather than only the first. The path names the object, so the field is identified by the message.
    try
    {
        (void) deserialize<triple>(parse("{ }"), triple_formats(), collecting());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(3U, err.problems().size());
        ensure_eq(std::string("Missing required field a"), err.problems().at(0).message());
        ensure_eq(std::string("Missing required field b"), err.problems().at(1).message());
        ensure_eq(std::string("Missing required field c"), err.problems().at(2).message());
    }
}

TEST(deserialize_collect_all_gathers_every_bad_element)
{
    try
    {
        (void) deserialize<std::vector<std::int64_t>>(parse(R"([ 1, "x", 3, "y" ])"), triple_formats(), collecting());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(2U, err.problems().size());
        ensure_eq(std::string("[1] [3]"), problem_paths(err));
    }
}

TEST(deserialize_collect_all_resumes_after_a_bridged_element_over_text)
{
    // Over text there is no tree for a bridged adapter to borrow, so the bridge materialises the element -- and
    // materialising is what walks the cursor over it. A failure then leaves the cursor already past the element,
    // where every other failure leaves it *on* the element, so a container recovering with a plain `next_value`
    // steps over the following sibling as well and loses both it and whatever it had to report.
    deserialization_context cxt(triple_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
    auto               rdr = open(R"([ { "a": "x", "b": 2, "c": 3 },
                                       { "a": 1,   "b": 2, "c": 3 },
                                       { "a": "y", "b": 2, "c": 3 }
                                     ])"
                                 );

    ensure(!cxt.deserialize<std::vector<triple>>(rdr).has_value());
    ensure_eq(2U, cxt.problems().size());
    ensure_eq(path::create("[0].a"), cxt.problems().at(0).path());
    ensure_eq(path::create("[2].a"), cxt.problems().at(1).path());
}

/// The paths a `collect_all` deserialization of a `T` out of \a source reports, which must be a failure.
template <typename T>
std::string collected_problem_paths(const std::string& source, const formats& fmts)
{
    try
    {
        (void) deserialize<T>(source, fmts, collecting());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        return problem_paths(err);
    }

    return std::string();
}

TEST(deserialize_collect_all_resumes_after_a_structure_which_failed_to_read)
{
    // Materialising a structure out of text walks the cursor into it, so a scalar inside which cannot be read -- a
    // number no `double` holds, an escape which does not decode -- used to leave the cursor there. The array holding
    // the structure then took the structure's own close for its own, and every element after it went unread.
    const std::string source = R"([ [ 1e400 ], [ 1 ], { "k": [ "\uD800" ] }, [ )"
                             + std::string(400, '9')
                             + " ], [ 2 ] ]";

    // Each of the deserializers which materialise a structure: the one for `value` itself, the `value` bridge, and
    // `coerce`'s way of turning a structure into a string.
    ensure_eq(std::string("[0] [2] [3]"),
              collected_problem_paths<std::vector<value>>(
                  source,
                  formats_builder().register_container<std::vector<value>>().compose_checked(formats::defaults())
              )
             );
    ensure_eq(std::string("[0] [2]"),
              collected_problem_paths<std::vector<bridged_triple>>(
                  R"([ { "a": 1e400, "b": 2, "c": 3 },
                       { "a": 1,     "b": 2, "c": 3 },
                       { "a": [ "\uD800" ], "b": 2, "c": 3 },
                       { "a": 1,     "b": 2, "c": 3 }
                     ])",
                  formats_builder().register_container<std::vector<bridged_triple>>().compose_checked(bridged_formats())
              )
             );
    ensure_eq(std::string("[0] [2] [3]"),
              collected_problem_paths<std::vector<std::string>>(
                  source,
                  formats_builder().register_container<std::vector<std::string>>().compose_checked(formats::coerce())
              )
             );
}

TEST(deserialize_collect_all_resumes_after_a_bridged_result_which_failed_to_move)
{
    // The bridge commits -- stepping the cursor past the value, or acknowledging that materialising it already did --
    // and only then returns, which moves the result with the caller's own move constructor. A failure there leaves the
    // value behind the cursor, so recovering by stepping over it again skips the sibling after it and reports the one
    // after that under its index. Text and a `value` reach the commit having advanced at different points: a structure
    // read from text is walked past to be materialised, where one in a tree is lent and only stepped over by `commit`.
    const formats fmts = formats_builder()
                             .register_container<std::vector<bridged_fragile>>()
                         .compose_checked(bridged_formats());

    for (bool from_value : { false, true })
    {
        auto collected = [&] (const std::string& text)
                         {
                             const value        tree = parse(text);
                             deserialization_context cxt(fmts, std::nullopt, jsonv::path(), nullptr, collecting());
                             reader             rdr  = from_value ? reader::from_value(tree) : reader(text);
                             (void) rdr.next_token();

                             ensure(!cxt.deserialize<std::vector<bridged_fragile>>(rdr).has_value());
                             return cxt.problems();
                         };

        // `[1]` is valid and has to be read; `[2]` fails in the older body, which still names the value it failed on.
        auto problems = collected(R"([ { "v": 9 }, { "v": 1 }, { "v": "x" } ])");
        ensure_eq(2U, problems.size());
        ensure_eq(path::create("[0]"), problems.at(0).path());
        ensure(problems.at(0).message().find("move refuses") != std::string::npos);
        ensure_eq(path::create("[2]"), problems.at(1).path());

        // On the last element, stepping again walks off the `]` and reports an array which never ended.
        problems = collected(R"([ { "v": 1 }, { "v": 9 } ])");
        ensure_eq(1U, problems.size());
        ensure_eq(path::create("[1]"), problems.at(0).path());
    }
}

TEST(deserialize_collect_all_records_a_nested_failure_exactly_once)
{
    // The interesting one. `deserialization_context::deserialize` moves the problems it collected off the context and
    // into the exception it throws, so a fold at both the member loop and the element loop would report every failure
    // twice. Two levels of recovery over four failures must still be four problems.
    value source = parse(R"([ { "a": "x", "b": "y", "c": 3 },
                              { "a": 1,   "b": 2,   "c": 3 },
                              { "a": "x", "b": "y", "c": 3 }
                            ])"
                        );

    try
    {
        (void) deserialize<std::vector<triple>>(source, triple_formats(), collecting());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(4U, err.problems().size());
        ensure_eq(std::string("[0].a [0].b [2].a [2].b"), problem_paths(err));
    }
}

TEST(deserialize_collect_all_stops_at_max_failures)
{
    value source = parse(R"([ "a", "b", "c", "d", "e" ])");

    try
    {
        (void) deserialize<std::vector<std::int64_t>>(source, triple_formats(), collecting(3U));
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(3U, err.problems().size());
        ensure_eq(std::string("[0] [1] [2]"), problem_paths(err));
    }
}

TEST(deserialize_collect_all_max_failures_boundary_is_fail_immediately)
{
    // A budget of 0 or 1 makes the first problem the last, which is `fail_immediately` in all but name -- and, note,
    // still reports one problem rather than none.
    for (auto limit : { deserialize_options::size_type(0U), deserialize_options::size_type(1U) })
    {
        try
        {
            (void) deserialize<triple>(parse(R"({ "a": "x", "b": "y", "c": "z" })"),
                                       triple_formats(),
                                       collecting(limit)
                                      );
            ensure(!"deserialization_error was not thrown");
        }
        catch (const deserialization_error& err)
        {
            ensure_eq(1U, err.problems().size());
            ensure_eq(path::create(".a"), err.problems().at(0).path());
        }
    }
}

TEST(deserialize_collect_all_terminates_without_an_enclosing_composite)
{
    // Nothing knows where to resume, so there is no loop to ask and the failure ends deserialization. Collecting is
    // something a composite opts into, not something the context can deliver on its own.
    try
    {
        (void) deserialize<std::int64_t>(parse(R"("x")"), formats::defaults(), collecting());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(1U, err.problems().size());
    }
}

TEST(deserialize_collect_all_still_throws_when_it_recovered)
{
    // Recovering collects diagnostics; it does not hand back a half-built object. An array of five where only the
    // third is bad still fails rather than yielding the other four.
    ensure_throws(deserialization_error,
                  deserialize<std::vector<std::int64_t>>(parse(R"([ 1, 2, "x", 4, 5 ])"),
                                                         triple_formats(),
                                                         collecting()
                                                        )
                 );
}

TEST(deserialize_collect_all_recovers_from_a_default_factory_failure)
{
    // A member's default factory is user code running outside the member's own deserialization, so it fails with
    // `std::out_of_range` rather than a `deserialization_error`. Recovery has to cover that too, or which members get
    // looked at would depend on how the first failing one happened to fail.
    try
    {
        (void) deserialize<triple>(parse(R"({ "b": "x", "c": "y" })"), seeded_triple_formats(), collecting());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        // Document order, not declaration order: `a` is absent, so its factory runs in the pass over the members no
        // key claimed, which is after the walk `b` and `c` failed during.
        ensure_eq(3U, err.problems().size());
        ensure_eq(std::string(".b .c .a"), problem_paths(err));
    }
}

TEST(deserialize_collect_all_max_failures_is_a_threshold_not_a_truncation)
{
    // A single failure which reports several problems at once is folded on whole rather than torn in half, so the
    // final list can exceed the limit by that batch. Pinned because it is the documented behaviour, not an accident:
    // truncating would drop diagnostics to enforce a bound whose point is to stop the walk, not to edit the report.
    try
    {
        (void) deserialize<batched>(parse("5"), batched_formats(), collecting(2U));
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(3U, err.problems().size());
    }
}

TEST(deserialize_default_factory_failure_is_untouched_when_not_collecting)
{
    // The new handler declines before recording anything and rethrows the original, so `fail_immediately` sees the
    // same exception reaching the same translation it always did -- with the factory's own exception kept as the
    // cause.
    try
    {
        (void) deserialize<triple>(parse(R"({ "b": 2, "c": 3 })"), seeded_triple_formats());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure(err.problems().at(0).nested_ptr());
        ensure_throws(std::out_of_range, (std::rethrow_exception(err.problems().at(0).nested_ptr()), 0));
    }
}

TEST(deserialize_collect_all_recovered_factory_failure_names_its_member)
{
    // A caller which walked the reader to `.child` itself has told the reader where it is and not the context, and
    // the two are arbitrated by `deserialization_context::path_scope`: a live scope is authoritative and the reader is
    // not consulted, so this names the member rather than the document position. That is what makes the recovered
    // problem say which default factory blew up, which the un-recovered path cannot -- it reports where the reader
    // was and not which member was being built.
    value  source = parse(R"({ "child": { "b": 2, "c": 3 } })");
    reader rdr    = reader::from_value(source);

    (void) rdr.next_token();   // onto the outer object
    (void) rdr.next_token();   // onto the key "child"
    (void) rdr.next_token();   // onto the child object itself

    deserialization_context cxt(seeded_triple_formats(), std::nullopt, jsonv::path(), nullptr, collecting());

    auto result = cxt.deserialize<triple>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a"), cxt.problems().at(0).path());
    ensure(cxt.problems().at(0).nested_ptr());
}

TEST(deserialize_factory_failure_outside_collect_all_still_reports_where_the_reader_was)
{
    // The mirror of the test above: with nothing to recover into, the original exception unwinds to the bridge and
    // is located from the reader, exactly as it was before this handler existed.
    value  source = parse(R"({ "child": { "b": 2, "c": 3 } })");
    reader rdr    = reader::from_value(source);

    (void) rdr.next_token();
    (void) rdr.next_token();
    (void) rdr.next_token();

    deserialization_context cxt(seeded_triple_formats());

    auto result = cxt.deserialize<triple>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".child"), cxt.problems().at(0).path());
}

TEST(deserialize_context_carries_its_options)
{
    deserialization_context defaulted(formats::defaults());
    ensure(defaulted.options().failure_mode() == deserialize_options::on_error::fail_immediately);
    ensure(!defaulted.recover());

    deserialization_context collector(formats::defaults(), std::nullopt, jsonv::path(), nullptr, collecting(2U));
    ensure(collector.options().failure_mode() == deserialize_options::on_error::collect_all);

    // Nothing recorded yet, so there is room; after two there is not.
    ensure(collector.recover());
    (void) collector.problem(jsonv::path(), "first");
    ensure(collector.recover());
    (void) collector.problem(jsonv::path(), "second");
    ensure(!collector.recover());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// jsonv::deserialize entry points                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

/// The node a deserializer was handed on entry, so a test can check it was never `document_start`.
struct entry_probe
{
    ast_node_type type;
};

formats probe_formats()
{
    static auto instance = make_deserializer([] (reader& from) -> entry_probe
                                             {
                                                 entry_probe out{ from.current().type() };
                                                 (void) from.next_value();
                                                 return out;
                                             });

    formats out = formats::compose({ formats::defaults() });
    out.register_deserializer(&instance);
    return out;
}

/// What a deserializer could see of the context it was handed.
struct context_probe
{
    std::optional<version> ver;
    const void*            user_data;
    jsonv::path            where;
};

formats context_probe_formats()
{
    static auto instance = make_deserializer([] (deserialization_context& context, reader& from) -> context_probe
                                             {
                                                 context_probe out{ context.version(),
                                                                    context.user_data(),
                                                                    context.path() };
                                                 (void) from.next_value();
                                                 return out;
                                             });

    formats out = formats::compose({ formats::defaults() });
    out.register_deserializer(&instance);
    return out;
}

/// Counts the instances alive, so a test can see that an object built and then refused was destroyed.
struct live_counted
{
    static inline int live = 0;

    live_counted()                    { ++live; }
    live_counted(const live_counted&) { ++live; }
    live_counted(live_counted&&)      { ++live; }
    ~live_counted()                   { --live; }
};

/// Builds a \c live_counted and, breaking the rule a deserializer is held to, leaves the cursor on the value it read.
/// That is what makes the whole-document check fail only once there is an object to destroy.
formats lazy_live_counted_formats()
{
    static auto instance = make_deserializer([] (reader&) -> live_counted { return live_counted(); });

    formats out = formats::compose({ formats::defaults() });
    out.register_deserializer(&instance);
    return out;
}

formats view_holder_formats()
{
    static formats instance = formats_builder()
                                  .type<view_holder>()
                                      .member("s", &view_holder::s)
                                  .register_container<std::vector<std::string_view>>()
                              .compose_checked(formats::defaults());
    return instance;
}

/// Long enough that no small-string buffer holds it, so a view which outlived its storage reads freed heap memory --
/// which is what ASan catches.
constexpr std::string_view long_text = "a string long enough to need the heap rather than the small-string buffer";

std::string quoted(std::string_view text)
{
    return "\"" + std::string(text) + "\"";
}

/// Run \a deserialize_and_read, which deserializes a view of a source freed when the deserialization returns and then
/// reads it, and check the deserialization refused rather than handing the view back.
template <typename FDeserializeAndRead>
void ensure_view_refused(const FDeserializeAndRead& deserialize_and_read)
{
    try
    {
        deserialize_and_read();
        ensure(!"a view of a source freed on return was not refused");
    }
    catch (const deserialization_error& err)
    {
        ensure(err.problems().at(0).message().find("std::string_view") != std::string::npos);
    }
}

/// What \c jsonv::parse says is wrong with \a text.
std::string parse_error_of(std::string_view text, const parse_options& options = parse_options())
{
    try
    {
        (void) parse(text, options);
    }
    catch (const parse_error& ex)
    {
        return ex.what();
    }

    ensure(!"parse_error was not thrown");
    return std::string();
}

/// Run \a deserialize, which deserializes from text that did not parse, and check it reported \a expected -- what the
/// parse said was wrong -- as the one problem, with the \c parse_error as its cause. A \c parse_error escaping instead
/// fails the test, since it is not caught here.
template <typename FDeserialize>
void ensure_parse_failure(const std::string& expected, const FDeserialize& deserialize)
{
    try
    {
        deserialize();
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(1U, err.problems().size());

        const auto& problem = err.problems().at(0);
        ensure(problem.message().find(expected) != std::string::npos);
        ensure(problem.message().find("Read node of type") == std::string::npos);
        ensure_throws(parse_error, (std::rethrow_exception(problem.nested_ptr()), 0));
    }
}

}

TEST(deserialize_entry_points_accept_every_source)
{
    // Most of what this checks is that each call compiles and is unambiguous. A C++ string also reaches
    // `deserialize(const value&)`, through a user-defined conversion, so the text overloads have to win outright.
    const char*           c_string   = "5";
    std::string           text       = "5";
    const std::string     const_text = "5";
    std::string_view      view       = text;
    const formats         fmts       = formats::defaults();
    const parse_options   popts      = parse_options::create_default();
    const deserialize_options eopts      = deserialize_options::create_default();

    ensure_eq(5, deserialize<std::int32_t>("5"));
    ensure_eq(5, deserialize<std::int32_t>(c_string));
    ensure_eq(5, deserialize<std::int32_t>(text));
    ensure_eq(5, deserialize<std::int32_t>(const_text));
    ensure_eq(5, deserialize<std::int32_t>(std::string("5")));
    ensure_eq(5, deserialize<std::int32_t>(view));

    ensure_eq(5, deserialize<std::int32_t>(view, fmts));
    ensure_eq(5, deserialize<std::int32_t>(view, eopts));
    ensure_eq(5, deserialize<std::int32_t>(view, fmts, eopts));
    ensure_eq(5, deserialize<std::int32_t>(view, popts));
    ensure_eq(5, deserialize<std::int32_t>(view, popts, fmts));
    ensure_eq(5, deserialize<std::int32_t>(view, popts, eopts));
    ensure_eq(5, deserialize<std::int32_t>(view, popts, fmts, eopts));
    ensure_eq(5, deserialize<std::int32_t>(std::string("5"), popts, fmts, eopts));

    {
        reader rdr("5");
        ensure_eq(5, deserialize<std::int32_t>(rdr));
    }
    {
        reader rdr("5");
        ensure_eq(5, deserialize<std::int32_t>(rdr, fmts));
    }
    {
        reader rdr("5");
        ensure_eq(5, deserialize<std::int32_t>(rdr, eopts));
    }
    {
        reader rdr("5");
        ensure_eq(5, deserialize<std::int32_t>(rdr, fmts, eopts));
    }
    ensure_eq(5, deserialize<std::int32_t>(reader("5")));
    ensure_eq(5, deserialize<std::int32_t>(reader("5"), fmts));
    ensure_eq(5, deserialize<std::int32_t>(reader("5"), eopts));
    ensure_eq(5, deserialize<std::int32_t>(reader("5"), fmts, eopts));

    // A context the caller built, in place of the formats and options. Nothing is recorded on one by a success, so one
    // serves every call here.
    deserialization_context cxt(fmts);
    ensure_eq(5, deserialize<std::int32_t>("5", cxt));
    ensure_eq(5, deserialize<std::int32_t>(c_string, cxt));
    ensure_eq(5, deserialize<std::int32_t>(text, cxt));
    ensure_eq(5, deserialize<std::int32_t>(const_text, cxt));
    ensure_eq(5, deserialize<std::int32_t>(std::string("5"), cxt));
    ensure_eq(5, deserialize<std::int32_t>(view, cxt));
    ensure_eq(5, deserialize<std::int32_t>(view, popts, cxt));
    ensure_eq(5, deserialize<std::int32_t>(std::string("5"), popts, cxt));
    {
        reader rdr("5");
        ensure_eq(5, deserialize<std::int32_t>(rdr, cxt));
    }
    ensure_eq(5, deserialize<std::int32_t>(reader("5"), cxt));
}

TEST(deserialize_entry_points_accept_every_source_for_a_dsl_type)
{
    const std::string doc      = R"({ "a": 1, "b": 2, "c": 3 })";
    const triple      expected = { 1, 2, 3 };
    const formats     fmts     = triple_formats();

    ensure_eq(expected, deserialize<triple>(doc, fmts));
    ensure_eq(expected, deserialize<triple>(std::string(doc), fmts));
    ensure_eq(expected, deserialize<triple>(doc.c_str(), fmts));
    ensure_eq(expected, deserialize<triple>(doc, parse_options(), fmts));
    ensure_eq(expected, deserialize<triple>(doc, fmts, deserialize_options()));
    ensure_eq(expected, deserialize<triple>(doc, parse_options(), fmts, deserialize_options()));
    ensure_eq(expected, deserialize<triple>(reader(doc), fmts));
    ensure_eq(expected, deserialize<triple>(reader(doc), fmts, deserialize_options()));
    {
        reader rdr(doc);
        ensure_eq(expected, deserialize<triple>(rdr, fmts));
    }
    {
        deserialization_context cxt(fmts);
        ensure_eq(expected, deserialize<triple>(doc, cxt));
        ensure_eq(expected, deserialize<triple>(std::string(doc), cxt));
        ensure_eq(expected, deserialize<triple>(doc.c_str(), cxt));
        ensure_eq(expected, deserialize<triple>(doc, parse_options(), cxt));
        ensure_eq(expected, deserialize<triple>(reader(doc), cxt));

        reader rdr(doc);
        ensure_eq(expected, deserialize<triple>(rdr, cxt));
    }

    // The shapes which take no `formats` use the global one.
    formats::set_global(triple_formats());
    auto reset_global_on_exit = jsonv::detail::on_scope_exit([] { formats::reset_global(); });

    ensure_eq(expected, deserialize<triple>(doc));
    ensure_eq(expected, deserialize<triple>(doc, deserialize_options()));
    ensure_eq(expected, deserialize<triple>(doc, parse_options()));
    ensure_eq(expected, deserialize<triple>(doc, parse_options(), deserialize_options()));
    ensure_eq(expected, deserialize<triple>(reader(doc)));
    ensure_eq(expected, deserialize<triple>(reader(doc), deserialize_options()));
    {
        reader rdr(doc);
        ensure_eq(expected, deserialize<triple>(rdr, deserialize_options()));
    }
}

TEST(deserialize_through_a_caller_context_hands_deserializers_what_it_was_built_with)
{
    // The overloads taking `formats` and `deserialize_options` build a context with no version, user data or base path,
    // so until a context could be passed in, nothing deserialized from a reader or from text could be given one.
    const int          token = 0;
    deserialization_context cxt(context_probe_formats(), version(2, 1), path::create(".outer"), &token);

    auto check = [&] (const context_probe& seen)
                 {
                     ensure(seen.ver == version(2, 1));
                     ensure(seen.user_data == &token);
                     ensure_eq(path::create(".outer"), seen.where);
                 };

    check(deserialize<context_probe>("{}", cxt));
    check(deserialize<context_probe>(std::string("{}"), cxt));
    check(deserialize<context_probe>("{}", parse_options(), cxt));
    check(deserialize<context_probe>(reader("{}"), cxt));

    reader rdr("{}");
    check(deserialize<context_probe>(rdr, cxt));
}

TEST(deserialize_through_a_caller_context_takes_its_problems_with_the_exception)
{
    // Whatever the context held before the call is the caller's; what the call recorded leaves with the exception, so
    // nothing is reported twice by a caller which reads both.
    deserialization_context cxt(formats::defaults());
    (void) cxt.problem(path(), "already there");

    try
    {
        (void) deserialize<std::int64_t>(R"("five")", cxt);
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure(err.problems().at(0).message().find("expecting integer") != std::string::npos);
    }

    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("already there"), cxt.problems().at(0).message());
}

TEST(deserialize_a_cpp_string_is_json_text)
{
    ensure_eq("fire", deserialize<std::string>(R"("fire")"));
    ensure_throws(deserialization_error, deserialize<std::string>("fire"));

    // To mean the JSON string, say so.
    ensure_eq("fire", deserialize<std::string>(value("fire")));

    // Anything which is not a string still converts to a `value`, as it always has.
    ensure_eq(5, deserialize<std::int64_t>(5));
    ensure(deserialize<bool>(true));
    ensure_eq(4.5, deserialize<double>(4.5));
}

TEST(deserialize_steps_onto_the_value_exactly_once)
{
    // A deserializer is never handed `document_start`, whichever entry point it was reached through, and the caller
    // does not have to step off it first.
    const formats fmts = probe_formats();

    ensure(deserialize<entry_probe>("5", fmts).type == ast_node_type::integer);
    ensure(deserialize<entry_probe>(std::string("5"), fmts).type == ast_node_type::integer);
    ensure(deserialize<entry_probe>(reader("5"), fmts).type == ast_node_type::integer);
    ensure(deserialize<entry_probe>(reader::from_value(value(5)), fmts).type == ast_node_type::integer);
    ensure(deserialize<entry_probe>(value(5), fmts).type == ast_node_type::integer);

    reader rdr("5");
    ensure(deserialize<entry_probe>(rdr, fmts).type == ast_node_type::integer);

    // The whole document was read, and the reader is left on its end.
    ensure(rdr.good());
    ensure(rdr.current().type() == ast_node_type::document_end);

    ensure_eq(5, deserialize<std::int32_t>(reader("5")));
}

TEST(deserialize_from_a_positioned_reader_reads_one_value)
{
    // A reader the caller has already moved is not a whole document: the value under the cursor is deserialized and the
    // cursor left one past it, whatever surrounds it -- which is what lets a caller walk an array element by element.
    const formats fmts = probe_formats();

    reader rdr = open("[ 7, 8 ]");
    (void) rdr.next_token();

    ensure(deserialize<entry_probe>(rdr, fmts).type == ast_node_type::integer);
    ensure_eq(8, deserialize<std::int64_t>(rdr));
    ensure(rdr.current().type() == ast_node_type::array_end);

    // Stepped onto the value by hand, it is not stepped over a second time.
    ensure_eq(5, deserialize<std::int64_t>(open("5")));
}

TEST(deserialize_malformed_text_reports_the_parse_failure)
{
    for (std::string_view text : { "[ 1, 2", "5 x", "5 6", R"({ "a": })" })
    {
        const std::string expected = parse_error_of(text);

        ensure_parse_failure(expected, [&] { (void) deserialize<value>(text); });
        ensure_parse_failure(expected, [&] { (void) deserialize<value>(std::string(text)); });
        ensure_parse_failure(expected, [&] { (void) deserialize<value>(reader(text)); });
        ensure_parse_failure(expected, [&] { reader rdr(text); (void) deserialize<value>(rdr); });
        ensure_parse_failure(expected, [&] { (void) deserialize<triple>(text, triple_formats()); });
    }
}

TEST(deserialize_reports_a_parse_which_failed_before_the_document_began)
{
    // A `max_structure_depth` of 0 refuses the document itself, so the parse fails before writing `document_start`
    // and the tape is a lone `error` node -- nothing an entry point would recognise as the start of a document.
    const parse_options options  = parse_options().max_structure_depth(0);
    const std::string   expected = parse_error_of("5", options);

    ensure_parse_failure(expected, [&] { (void) deserialize<std::int64_t>("5", options); });
    ensure_parse_failure(expected, [&] { reader rdr("5", options); (void) deserialize<std::int64_t>(rdr); });
}

TEST(deserialize_from_a_reader_positioned_on_a_parse_failure_reports_it)
{
    // Part-way through a document there is no `document_start` to validate at, but a cursor on the `error` node has
    // nothing to deserialize either. What the parse said is more use than what a deserializer expected to find instead.
    reader rdr("[ 1, x ]");
    (void) rdr.next_token();
    (void) rdr.next_token();
    (void) rdr.next_token();
    ensure(rdr.current().type() == ast_node_type::error);

    ensure_parse_failure(parse_error_of("[ 1, x ]"), [&] { (void) deserialize<std::int64_t>(rdr); });
}

TEST(deserialize_converts_text_as_the_value_category_it_was_given)
{
    // The constraint accepts whatever converts to `std::string_view` as the argument was passed, so the conversion has
    // to be made that way too: a type may convert only as an rvalue, or to different text as each.
    struct rvalue_only_text
    {
        operator std::string_view() && { return "5"; }
    };

    struct text_by_category
    {
        operator std::string_view() const& { return "1"; }
        operator std::string_view() &&     { return "2"; }
    };

    ensure_eq(5, deserialize<std::int32_t>(rvalue_only_text{}));

    text_by_category named;
    ensure_eq(1, deserialize<std::int32_t>(named));
    ensure_eq(2, deserialize<std::int32_t>(text_by_category{}));
}

TEST(deserialize_refuses_a_deserializer_which_leaves_its_value_unread)
{
    // The same check is what catches a deserializer breaking the rule it is held to -- here, one which reads nothing --
    // and by then it has built an object, which has to be destroyed on the way out.
    const formats fmts = lazy_live_counted_formats();

    for (const auto& run : { +[] (const formats& f) { (void) deserialize<live_counted>("5", f); },
                             +[] (const formats& f) { (void) deserialize<live_counted>(value(5), f); },
                           })
    {
        try
        {
            run(fmts);
            ensure(!"deserialization_error was not thrown");
        }
        catch (const deserialization_error& err)
        {
            ensure_eq(1U, err.problems().size());
            ensure(err.problems().at(0).message().find("end of document") != std::string::npos);
        }

        ensure_eq(0, live_counted::live);
    }
}

TEST(deserialize_refuses_views_of_a_source_it_was_handed)
{
    // A source handed over to deserialization is freed when the call returns, so a view of it would dangle immediately.
    // If one got through, reading it is a use-after-free for ASan to report rather than a test which quietly passes.
    const std::string document = quoted(long_text);
    const std::string object   = R"({ "s": )" + document + " }";
    const std::string array    = "[ " + document + " ]";

    ensure_view_refused([&] { ensure_eq(long_text, deserialize<std::string_view>(std::string(document))); });
    ensure_view_refused([&]
                        {
                            auto held = deserialize<view_holder>(std::string(object), view_holder_formats());
                            ensure_eq(long_text, held.s);
                        });
    ensure_view_refused([&]
                        {
                            auto views = deserialize<std::vector<std::string_view>>(std::string(array),
                                                                                    view_holder_formats()
                                                                                   );
                            ensure_eq(long_text, views.at(0));
                        });

    // The same through a reader which owns what it reads and dies with the call.
    ensure_view_refused([&] { ensure_eq(long_text, deserialize<std::string_view>(reader(std::string(document)))); });
    ensure_view_refused([&]
                        {
                            std::string copy(long_text);
                            auto        view = deserialize<std::string_view>(reader::from_value(value(copy)));
                            ensure_eq(long_text, view);
                        });

    // The same through a context the caller built, which is not left believing its next source is temporary too.
    deserialization_context cxt;
    ensure_view_refused([&] { ensure_eq(long_text, deserialize<std::string_view>(std::string(document), cxt)); });
    ensure_view_refused([&]
                        {
                            ensure_eq(long_text, deserialize<std::string_view>(reader(std::string(document)), cxt));
                        });
    ensure(!cxt.source_is_temporary());

    // A copy is fine.
    ensure_eq(long_text, deserialize<std::string>(std::string(document)));
}

TEST(deserialize_views_a_source_the_caller_keeps)
{
    const std::string document = quoted(long_text);
    const std::string object   = R"({ "s": )" + document + " }";

    auto inside = [] (const std::string& source, std::string_view view)
                  {
                      return view.data() >= source.data() && view.data() + view.size() <= source.data() + source.size();
                  };

    ensure(inside(document, deserialize<std::string_view>(std::string_view(document))));
    ensure(inside(document, deserialize<std::string_view>(document)));
    ensure(inside(document, deserialize<std::string_view>(reader(std::string_view(document)))));
    ensure(inside(object, deserialize<view_holder>(object, view_holder_formats()).s));

    // An owning reader the caller holds outlives the call, so views of it are the caller's to keep valid.
    reader owning{ std::string(document) };
    ensure_eq(long_text, deserialize<std::string_view>(owning));

    // The same through a context the caller built.
    deserialization_context cxt;
    ensure(inside(document, deserialize<std::string_view>(document, cxt)));
    ensure(inside(document, deserialize<std::string_view>(reader(std::string_view(document)), cxt)));
    reader owning_again{ std::string(document) };
    ensure_eq(long_text, deserialize<std::string_view>(owning_again, cxt));
}

TEST(deserialize_honours_parse_options)
{
    // Comments are refused unless turned on, so turning them on is the non-default worth checking.
    ensure_throws(deserialization_error, deserialize<std::int64_t>("/* c */ 5"));
    ensure_eq(5, deserialize<std::int64_t>("/* c */ 5", parse_options().comments(true)));

    try
    {
        (void) deserialize<std::int64_t>("5", parse_options().require_document(true));
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure(err.problems().at(0).message().find("Could not parse JSON") != std::string::npos);
    }

    const triple expected = { 1, 2, 3 };
    ensure_eq(expected,
              deserialize<triple>(R"(/* c */ { "a": 1, "b": 2, "c": 3 })",
                                  parse_options().comments(true),
                                  triple_formats())
             );

    // Both sets of options at once, each doing its own job.
    try
    {
        (void) deserialize<triple>(R"(/* c */ { "a": "x", "b": "y", "c": 3 })",
                                   parse_options().comments(true),
                                   triple_formats(),
                                   collecting()
                                  );
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(std::string(".a .b"), problem_paths(err));
    }
}

TEST(deserialize_value_round_trips_through_every_source)
{
    const std::string text     = R"({ "i": 5, "a": [ 1, 2.5, "three", null, true ], "o": { "x": {} } })";
    const value       expected = parse(text);

    ensure_eq(expected, deserialize<value>(text));
    ensure_eq(expected, deserialize<value>(std::string(text)));
    ensure_eq(expected, deserialize<value>(reader(text)));
    ensure_eq(expected, deserialize<value>(reader::from_value(expected)));
    ensure_eq(expected, deserialize<value>(expected));
    {
        reader rdr(text);
        ensure_eq(expected, deserialize<value>(rdr));
    }

    // The spelling through a parsed `value` and the one straight from text agree for a DSL type as well.
    const std::string doc = R"({ "a": 1, "b": 2, "c": 3 })";
    ensure_eq(deserialize<triple>(parse(doc), triple_formats()), deserialize<triple>(doc, triple_formats()));
}

}
