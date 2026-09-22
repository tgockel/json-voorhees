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
#include "test.hpp"

#include <jsonv/ast.hpp>
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
#include <vector>
#include <memory>
#include <new>
#include <string>
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

TEST(extract_string_view_from_text_is_refused_rather_than_dangling)
{
    // The bridge decodes text into a tree it owns, so a view of that tree would name freed storage the moment the
    // extraction unwinds. Reporting a problem is the honest answer; case 07 makes it work by viewing the source text.
    extraction_context cxt(formats::defaults());
    auto               rdr = open(R"("long enough to be on the heap rather than in the small-string buffer")");

    auto result = cxt.extract<std::string_view>(rdr);
    ensure(!result.has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure(cxt.problems().at(0).message().find("std::string_view") != std::string::npos);

    // `std::string` is the documented way to get the same text out of a text source, and still works.
    auto other = open(R"("long enough to be on the heap rather than in the small-string buffer")");
    extraction_context copied(formats::defaults());
    ensure_eq(std::string("long enough to be on the heap rather than in the small-string buffer"),
              *copied.extract<std::string>(other)
             );
}

TEST(extract_nested_string_view_from_text_is_refused)
{
    // A leaf-only check would miss this: the container materialises once, and each element then sees a value-backed
    // reader over that temporary and would happily borrow from it.
    formats fmts = formats::compose({ formats_builder()
                                          .register_container<std::vector<std::string_view>>(),
                                      formats::defaults()
                                    });

    extraction_context cxt(fmts);
    auto               rdr = open(R"([ "the first long string value here", "the second long string value here" ])");

    ensure(!cxt.extract<std::vector<std::string_view>>(rdr).has_value());
    ensure(!cxt.problems().empty());
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
        extraction_context cxt(formats::defaults());
        auto               rdr = open(R"({ "a": [ [], 99 ] })");

        while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
            (void) rdr.next_token();
        (void) rdr.next_token();   // onto the inner [], which is `.a[0]`

        auto result = cxt.extract<std::int64_t>(rdr);
        ensure(!result.has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_ne(path::create(".a[1]"), cxt.problems().at(0).path());
        ensure_eq(path::create(".a"), cxt.problems().at(0).path());
    }

    // Landing on a closing token is the case where `current_path` already names the structure, so nothing is dropped.
    {
        extraction_context cxt(formats::defaults());
        auto               rdr = open(R"({ "a": [ [] ] })");

        while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
            (void) rdr.next_token();
        (void) rdr.next_token();   // onto the inner []

        ensure(!cxt.extract<std::int64_t>(rdr).has_value());
        ensure_eq(path::create(".a"), cxt.problems().at(0).path());
    }
}

TEST(extract_stale_cursor_does_not_leak_into_a_nested_reader)
{
    // The bridge marks the reader it walked past, not a global flag: a nested extraction running on its own
    // value-backed reader must still get that reader's exact position.
    formats fmts = formats::compose({ formats_builder()
                                          .register_optional<std::optional<std::int64_t>>(),
                                      formats::defaults()
                                    });

    extraction_context cxt(fmts);
    auto               rdr = open(R"([ { "a": "wrong" }, 99 ])");
    (void) rdr.next_token();   // onto the object

    // The object is materialised (cursor moves past it); the member extraction below runs on a fresh value reader.
    ensure(!cxt.extract<std::optional<std::int64_t>>(rdr).has_value());
    ensure(!cxt.problems().empty());
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
    // -- and must not leave the location the bridge deposited lying around for the next failure to pick up.
    formats fmts = formats::compose({ formats_builder()
                                          .register_optional<std::optional<std::int64_t>>(),
                                      formats::defaults()
                                    });

    extraction_context cxt(fmts);
    auto               rdr = open(R"([ { "a": 1 }, "not a number" ])");
    (void) rdr.next_token();   // onto the object, which is `[0]`

    // The object is materialised, so the cursor moves past it; the nested extraction then throws an
    // `extraction_error`, which is folded rather than re-located.
    ensure(!cxt.extract<std::optional<std::int64_t>>(rdr).has_value());
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
    extraction_context cxt(formats::defaults());
    auto               rdr = open(R"({ "a": [ [], 99 ] })");

    while (rdr.good() && rdr.current().type() != ast_node_type::array_begin)
        (void) rdr.next_token();   // onto the `[` of `.a`
    (void) rdr.next_token();       // onto the inner `[]`, which is `.a[0]`

    // Reaching the extractor through `formats` is public, discouraged, and -- the point here -- skips the handler
    // which would have translated and consumed what the bridge leaves behind.
    alignas(std::int64_t) std::byte place[sizeof(std::int64_t)];
    try
    {
        (void) cxt.formats().extract(typeid(std::int64_t), rdr, static_cast<void*>(place), cxt);
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

}
