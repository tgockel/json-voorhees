/// \file
/// Tests for the built-in \c formats::defaults and \c formats::coerce extractors, which read the AST node the reader
/// is on rather than a \c value someone materialised for them.
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
#include <jsonv/parse.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization/adapter_for.hpp>
#include <jsonv/value.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

/// The \c T a built-in extracts from the JSON \a source text, which must succeed.
///
/// Everything here goes through a text-backed reader on purpose: that is the source with no tree behind it, so an
/// extractor which quietly reached for one would have nothing to reach for.
template <typename T>
T extracted(std::string_view source, const formats& fmts = formats::defaults())
{
    extraction_context cxt(fmts);
    reader             rdr(source);
    (void) rdr.next_token();

    auto out = cxt.extract<T>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    return std::move(*out);
}

/// The single problem a built-in reports for the JSON \a source text, which must fail.
///
/// One problem exactly: these are leaves, so anything which reported two would be describing one bad token twice.
template <typename T>
extraction_error::problem refused(std::string_view source, const formats& fmts = formats::defaults())
{
    extraction_context cxt(fmts);
    reader             rdr(source);
    (void) rdr.next_token();

    auto out = cxt.extract<T>(rdr);
    ensure(!out.has_value());
    ensure_eq(1U, cxt.problems().size());
    return cxt.problems().at(0);
}

bool mentions(const extraction_error::problem& problem, std::string_view text)
{
    return problem.message().find(text) != std::string::npos;
}

/// Add one to the magnitude of a decimal literal, so `127` becomes `128` and `-128` becomes `-129`.
///
/// This is how a test names a number one past the bound of a type which, by definition, cannot hold it.
std::string increment_magnitude(std::string text)
{
    for (auto digit = text.rbegin(); digit != text.rend() && *digit != '-'; ++digit)
    {
        if (*digit != '9')
        {
            ++*digit;
            return text;
        }

        *digit = '0';
    }

    // Every digit was a nine, so the magnitude grew a place: `999` is now `000` and wants a `1` in front of it.
    text.insert(text.find_first_of("0123456789"), 1U, '1');
    return text;
}

/// Zero, one, both bounds and one past each, for one integer width.
///
/// The unary `+` promotes the byte-sized types to `int`, so a mismatch prints as a number rather than as whatever
/// character it happens to spell.
template <typename T>
void check_integer_width()
{
    using limits = std::numeric_limits<T>;

    ensure_eq(+T(0), +extracted<T>("0"));
    ensure_eq(+T(1), +extracted<T>("1"));

    const std::string lowest  = std::to_string(limits::min());
    const std::string highest = std::to_string(limits::max());
    ensure_eq(+limits::min(), +extracted<T>(lowest));
    ensure_eq(+limits::max(), +extracted<T>(highest));

    ensure(mentions(refused<T>(increment_magnitude(highest)), "out of range"));

    if constexpr (limits::is_signed)
    {
        ensure_eq(+T(-1), +extracted<T>("-1"));
        ensure(mentions(refused<T>(increment_magnitude(lowest)), "out of range"));
    }
    else
    {
        // A negative literal is out of range for an unsigned type rather than its two's-complement reinterpretation.
        ensure(mentions(refused<T>("-1"), "out of range"));
    }
}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::defaults                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(builtin_integer_widths)
{
    check_integer_width<std::int8_t>();
    check_integer_width<std::uint8_t>();
    check_integer_width<std::int16_t>();
    check_integer_width<std::uint16_t>();
    check_integer_width<std::int32_t>();
    check_integer_width<std::uint32_t>();
    check_integer_width<std::int64_t>();
    check_integer_width<std::uint64_t>();

    // Usually aliases of the sized types above, but not on every platform the library builds for.
    check_integer_width<std::size_t>();
    check_integer_width<std::ptrdiff_t>();
    check_integer_width<long>();
    check_integer_width<unsigned long>();
}

TEST(builtin_integer_out_of_range_is_reported_rather_than_wrapped)
{
    // The whole point of reading the token against the destination: `ast_node::integer::value()` saturates a literal
    // it cannot hold, and a narrowing conversion then wraps that silently into something which looks like a number
    // the document contained.
    ensure(mentions(refused<std::uint8_t>("999"), "out of range"));
    ensure(mentions(refused<std::int8_t>("200"), "out of range"));

    // The two literals #206 reports as indistinguishable: one is exactly `UINT64_MAX` and the other is far past it.
    ensure_eq(std::numeric_limits<std::uint64_t>::max(), extracted<std::uint64_t>("18446744073709551615"));
    ensure(mentions(refused<std::uint64_t>("18446744073709551616"), "out of range"));
    ensure(mentions(refused<std::int64_t>("18446744073709551615"), "out of range"));
    ensure(mentions(refused<std::int64_t>("-99999999999999999999999"), "out of range"));
}

TEST(builtin_integer_from_a_value_reads_what_the_value_holds)
{
    // A `value` holds integers as `std::int64_t`, so a literal outside that range was already saturated when the
    // document was parsed. That is #206 and not something an extractor can undo -- what it can do is stop presenting
    // the saturated number as the one the document contained.
    ensure_eq(std::int64_t(-1), parse("18446744073709551615").as_integer());
    ensure_throws(extraction_error, extract<std::uint64_t>(parse("18446744073709551615")));

    // Which is the same refusal as for a document that really does say `-1`. `to_json` of a `std::uint64_t` above
    // `INT64_MAX` writes exactly that, so such a value no longer round-trips -- deliberately, since the JSON it
    // produces is a negative number and every reader of it is entitled to say so.
    ensure_eq(value(std::int64_t(-1)), to_json(std::numeric_limits<std::uint64_t>::max()));
    ensure_throws(extraction_error, extract<std::uint64_t>(parse("-1")));

    // Read from the text, the literal says what it says.
    ensure_eq(std::numeric_limits<std::uint64_t>::max(), extracted<std::uint64_t>("18446744073709551615"));
}

TEST(builtin_decimal_from_decimal_and_integer)
{
    ensure_eq(4.5, extracted<double>("4.5"));
    ensure_eq(5.0, extracted<double>("5"));
    ensure_eq(-5.0, extracted<double>("-5"));
    ensure_eq(1000.0, extracted<double>("1e3"));

    ensure_eq(4.5f, extracted<float>("4.5"));
    ensure_eq(5.0f, extracted<float>("5"));
}

TEST(builtin_decimal_keeps_negative_zero)
{
    double negative = extracted<double>("-0.0");
    ensure_eq(0.0, negative);
    ensure(std::signbit(negative));

    // An integer `-0` is a zero of the other sign, which is what the token says and what the parser records.
    ensure(!std::signbit(extracted<double>("0")));
}

TEST(builtin_decimal_keeps_seventeen_significant_digits)
{
    // Seventeen digits is the point at which a `double` distinguishes two neighbours, so a round trip through anything
    // narrower shows up here.
    ensure_eq(0.10000000000000001, extracted<double>("0.10000000000000001"));
    ensure_eq(1.2345678901234567, extracted<double>("1.2345678901234567"));
    ensure_eq(2.2250738585072014e-308, extracted<double>("2.2250738585072014e-308"));

    // An integer token beyond `std::int64_t` still rounds to the nearest `double` rather than to the bound
    // `ast_node::integer::value()` would saturate to.
    ensure_eq(1.2345678901234568e22, extracted<double>("12345678901234567890123"));
}

TEST(builtin_nonfinite_decimal_reads_the_value_not_its_rendering)
{
    // A `value` can hold a `double` that JSON cannot write, and a value-backed reader renders it as `null` -- which
    // is what encoding it produces and is the right answer for a document. It is the wrong answer for an extractor,
    // which is reading the tree and not an encoding of it, so the number is still there to be had.
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan      = std::numeric_limits<double>::quiet_NaN();

    ensure_eq(infinity, extract<double>(value(infinity)));
    ensure_eq(-infinity, extract<double>(value(-infinity)));
    ensure(std::isnan(extract<double>(value(nan))));

    // The coercing rules see the same thing, so a non-finite number is true rather than the falsehood a `null` is.
    const formats coerce = formats::coerce();
    ensure_eq(true, extract<bool>(value(infinity), coerce));
    ensure_eq(true, extract<bool>(value(nan), coerce));
    ensure_eq(std::numeric_limits<std::int64_t>::max(), extract<std::int64_t>(value(infinity), coerce));

    // A genuine null is unaffected: it is false to `coerce` and not a number to `defaults`.
    ensure_eq(false, extract<bool>(value(), coerce));
    ensure_throws(extraction_error, extract<double>(value()));

    // As is a finite number, which the rendering describes perfectly well.
    ensure_eq(4.5, extract<double>(value(4.5)));
    ensure_eq(true, extract<bool>(value(4.5), coerce));
    ensure_eq(false, extract<bool>(value(0.0), coerce));
}

TEST(builtin_boolean)
{
    ensure_eq(true, extracted<bool>("true"));
    ensure_eq(false, extracted<bool>("false"));
}

TEST(builtin_string_from_canonical_and_escaped)
{
    ensure_eq(std::string("thing"), extracted<std::string>(R"("thing")"));
    ensure_eq(std::string(""), extracted<std::string>(R"("")"));

    // The escaped path is the one which has decoding to do. Both sides stay ASCII: the JSON fixture says what it
    // is testing, and the expectation is spelt in explicit UTF-8 bytes because a narrow literal is encoded in
    // whatever the compiler's execution character set happens to be, which is not UTF-8 everywhere.
    ensure_eq(std::string("a\tb"), extracted<std::string>(R"("a\tb")"));
    ensure_eq(std::string("\xc3\xa9"), extracted<std::string>(R"("\u00e9")"));            // U+00E9

    // A surrogate pair is one code point spelt as two escapes.
    ensure_eq(std::string("\xf0\x9f\x98\x80"), extracted<std::string>(R"("\ud83d\ude00")"));   // U+1F600
}

TEST(builtin_string_bad_escape_is_an_extraction_problem_with_a_path)
{
    // `parse_index` validates that an escape is *syntactically* four hex digits without decoding it, so a well-formed
    // high surrogate with nothing to pair with only fails when the string is read. That belongs in the problem list
    // with a path, not as a `parse_error` thrown out of `extract`.
    auto problem = refused<std::string>(R"("\ud800")");
    ensure(mentions(problem, "surrogate"));
    ensure_eq(path(), problem.path());

    // The same failure inside a document, so the path has something to say.
    extraction_context cxt(formats::defaults());
    reader             rdr(R"({ "a": [ 1, "\ud800" ] })");
    (void) rdr.next_token();
    while (rdr.good() && rdr.current().type() != ast_node_type::string_escaped)
        (void) rdr.next_token();

    ensure(!cxt.extract<std::string>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[1]"), cxt.problems().at(0).path());
    ensure(mentions(cxt.problems().at(0), "surrogate"));
}

TEST(builtin_string_view_over_a_canonical_string)
{
    // Covered against a live source in `extract_string_view_from_text_points_at_the_source`; this is the value-backed
    // half, where the view has to name the caller's storage and not the token the reader synthesised for it.
    value source = std::string("long enough to be on the heap rather than in the small-string buffer");
    auto  view   = extract<std::string_view>(source);

    ensure_eq(source.as_string_view(), view);
    ensure(view.data() == source.as_string_view().data());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Type mismatches                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(builtin_type_mismatch_names_what_was_wanted_and_what_was_found)
{
    ensure_eq(std::string("Read node of type string when expecting integer"),
              refused<std::int32_t>(R"("nope")").message()
             );
    ensure_eq(std::string("Read node of type true when expecting integer"), refused<std::int32_t>("true").message());
    ensure_eq(std::string("Read node of type null when expecting integer"), refused<std::int32_t>("null").message());
    ensure_eq(std::string("Read node of type object when expecting integer"), refused<std::int32_t>("{}").message());
    ensure_eq(std::string("Read node of type array when expecting integer"), refused<std::int32_t>("[]").message());

    // A decimal is not an integer, even though an integer is a decimal.
    ensure_eq(std::string("Read node of type decimal when expecting integer"), refused<std::int32_t>("4.5").message());

    ensure_eq(std::string("Read node of type string when expecting one of decimal, integer"),
              refused<double>(R"("nope")").message()
             );
    ensure_eq(std::string("Read node of type string when expecting one of decimal, integer"),
              refused<float>(R"("nope")").message()
             );
    ensure_eq(std::string("Read node of type integer when expecting one of true, false"),
              refused<bool>("7").message()
             );
    // The two spellings of a string share a description, so the message says it once rather than listing how the
    // source might have encoded it.
    ensure_eq(std::string("Read node of type integer when expecting string"), refused<std::string>("7").message());
    ensure_eq(std::string("Read node of type integer when expecting string"),
              refused<std::string_view>("7").message()
             );
}

TEST(builtin_type_mismatch_carries_the_type_found_and_names_where_it_was)
{
    extraction_context cxt(formats::defaults());
    reader             rdr(R"({ "a": [ 1, "two" ] })");
    (void) rdr.next_token();
    while (rdr.good() && rdr.current().type() != ast_node_type::string_canonical)
        (void) rdr.next_token();

    auto result = cxt.extract<std::int32_t>(rdr);
    ensure(!result.has_value());

    // The error channel reports what was really there rather than the `error` sentinel, so a caller can branch on it.
    ensure(result.error() == ast_node_type::string_canonical);
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[1]"), cxt.problems().at(0).path());
}

TEST(builtin_out_of_range_names_where_it_was)
{
    extraction_context cxt(formats::defaults());
    reader             rdr(R"({ "a": [ 1, 999 ] })");
    (void) rdr.next_token();
    while (rdr.good() && rdr.current().token_raw() != "999")
        (void) rdr.next_token();

    ensure(!cxt.extract<std::uint8_t>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[1]"), cxt.problems().at(0).path());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// formats::coerce                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

template <typename T>
T coerced(std::string_view source)
{
    return extracted<T>(source, formats::coerce());
}

template <typename T>
extraction_error::problem coerce_refused(std::string_view source)
{
    return refused<T>(source, formats::coerce());
}

}

TEST(builtin_coerce_passes_the_strict_cases_through)
{
    ensure_eq(5, coerced<std::int8_t>("5"));
    ensure_eq(5, coerced<std::uint8_t>("5"));
    ensure_eq(5, coerced<std::int16_t>("5"));
    ensure_eq(5, coerced<std::uint16_t>("5"));
    ensure_eq(5, coerced<std::int32_t>("5"));
    ensure_eq(5U, coerced<std::uint32_t>("5"));
    ensure_eq(5, coerced<std::int64_t>("5"));
    ensure_eq(5U, coerced<std::uint64_t>("5"));
    ensure_eq(4.5f, coerced<float>("4.5"));
    ensure_eq(4.5, coerced<double>("4.5"));
    ensure_eq(std::string("10"), coerced<std::string>(R"("10")"));

    // `value` and `std::string_view` are not re-registered by `coerce`, so they come from the defaults underneath it.
    ensure_eq(parse(R"({ "a": 1 })"), coerced<value>(R"({ "a": 1 })"));
    ensure_eq(std::string_view("10"), coerced<std::string_view>(R"("10")"));
}

TEST(builtin_coerce_string_to_number)
{
    ensure_eq(10, coerced<std::int32_t>(R"("10")"));
    ensure_eq(10, coerced<std::uint8_t>(R"("10")"));
    ensure_eq(1000, coerced<std::int32_t>(R"("1e3")"));
    ensure_eq(7, coerced<std::int32_t>(R"("7.8")"));
    ensure_eq(7.8, coerced<double>(R"("7.8")"));
    ensure_eq(5, coerced<std::int32_t>(R"("  5  ")"));

    // A string which says nothing numeric is still a failure.
    ensure(!coerce_refused<std::int32_t>(R"("foo")").message().empty());
    ensure(!coerce_refused<std::int32_t>(R"("null")").message().empty());
    ensure(!coerce_refused<double>(R"("foo")").message().empty());

    // Out of range for the destination is a failure even though the string parses.
    ensure(mentions(coerce_refused<std::uint8_t>(R"("999")"), "out of range"));
}

TEST(builtin_coerce_number_to_string)
{
    ensure_eq(std::string("5"), coerced<std::string>("5"));
    ensure_eq(std::string("4.5"), coerced<std::string>("4.5"));
    ensure_eq(std::string("true"), coerced<std::string>("true"));
    ensure_eq(std::string("false"), coerced<std::string>("false"));
    ensure_eq(std::string("null"), coerced<std::string>("null"));

    // A composite coerces to its JSON encoding, which is the one case that has to read the subtree back out.
    ensure_eq(to_string(parse("[1,2,3]")), coerced<std::string>("[1,2,3]"));
    ensure_eq(to_string(parse(R"({ "a": 1 })")), coerced<std::string>(R"({ "a": 1 })"));
}

TEST(builtin_coerce_boolean_follows_the_python_rules)
{
    ensure_eq(false, coerced<bool>("null"));
    ensure_eq(true, coerced<bool>("true"));
    ensure_eq(false, coerced<bool>("false"));

    ensure_eq(false, coerced<bool>("0"));
    ensure_eq(false, coerced<bool>("-0"));
    ensure_eq(true, coerced<bool>("1"));
    ensure_eq(false, coerced<bool>("0.0"));
    ensure_eq(false, coerced<bool>("-0.0"));
    ensure_eq(true, coerced<bool>("0.5"));

    // A string is empty or it is not; `"false"` is not.
    ensure_eq(false, coerced<bool>(R"("")"));
    ensure_eq(true, coerced<bool>(R"("false")"));

    ensure_eq(false, coerced<bool>("[]"));
    ensure_eq(true, coerced<bool>("[ 1 ]"));
    ensure_eq(false, coerced<bool>("{}"));
    ensure_eq(true, coerced<bool>(R"({ "a": 1 })"));
}

TEST(builtin_coerce_number_and_boolean_conversions)
{
    ensure_eq(1, coerced<std::int32_t>("true"));
    ensure_eq(0, coerced<std::int32_t>("false"));
    ensure_eq(1.0, coerced<double>("true"));
    ensure_eq(0.0, coerced<double>("false"));

    // A decimal truncates toward zero on the way to an integer.
    ensure_eq(4, coerced<std::int32_t>("4.5"));
    ensure_eq(-4, coerced<std::int32_t>("-4.5"));
    ensure_eq(0, coerced<std::int32_t>("-0.2"));

    // An integer widens to a decimal without going through `std::int64_t`'s bounds.
    ensure_eq(5.0, coerced<double>("5"));
}

TEST(builtin_coerce_still_refuses_what_it_always_did)
{
    // `coerce_integer` and `coerce_decimal` have never accepted a null or a composite.
    ensure(!coerce_refused<std::int32_t>("null").message().empty());
    ensure(!coerce_refused<std::int32_t>("[1,2,3]").message().empty());
    ensure(!coerce_refused<std::int32_t>(R"({ "a": 1 })").message().empty());
    ensure(!coerce_refused<double>("null").message().empty());
    ensure(!coerce_refused<double>("[1,2,3]").message().empty());
    ensure(!coerce_refused<double>("{}").message().empty());

    // A decimal beyond the destination's range clamps to `std::int64_t` and is then out of range for the destination,
    // rather than wrapping into a number the document never mentioned.
    ensure(mentions(coerce_refused<std::uint8_t>("1e300"), "out of range"));
}

TEST(builtin_coerce_rejects_a_structure_the_document_never_closed)
{
    // Weighing a structure from the element count its opener carries is what makes `coerce()` cheap, and the count
    // is only about a value which exists if the parser reached the matching close. A truncated document leaves an
    // opener with a count on it all the same, and answering from that reports the truthiness of something the source
    // never finished writing.
    for (std::string_view truncated : { "[1,", "[", "{", R"({ "a": )", "[1,2,]",
                                        // An object whose first member has a key and a colon but no value. The
                                        // parser used to call this document successful, so the reader walked it
                                        // and the count claimed a member -- see
                                        // `ast_parse_object_missing_first_member_value`.
                                        R"({"a":})", R"([{"a":}])"
                                      })
    {
        extraction_context cxt(formats::coerce());
        reader             rdr(truncated);
        (void) rdr.next_token();

        auto result = cxt.extract<bool>(rdr);
        ensure(!result.has_value());
        ensure_eq(1U, cxt.problems().size());
    }

    // A structure which closed is still answered without being walked.
    ensure_eq(false, coerced<bool>("[]"));
    ensure_eq(true, coerced<bool>("[ 1 ]"));
    ensure_eq(false, coerced<bool>("{}"));
    ensure_eq(true, coerced<bool>(R"({ "a": 1 })"));
}

TEST(builtin_coerce_names_where_a_failure_was)
{
    extraction_context cxt(formats::coerce());
    reader             rdr(R"({ "a": [ 1, null ] })");
    (void) rdr.next_token();
    while (rdr.good() && rdr.current().type() != ast_node_type::literal_null)
        (void) rdr.next_token();

    auto result = cxt.extract<std::int32_t>(rdr);
    ensure(!result.has_value());
    ensure(result.error() == ast_node_type::literal_null);
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(path::create(".a[1]"), cxt.problems().at(0).path());
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

namespace
{

/// The allocations one extraction of a \c T out of the JSON \a source text performs, with everything the extraction
/// does not pay for -- parsing the text, building the context -- set up beforehand.
template <typename T>
std::size_t extraction_cost(std::string_view source, const formats& fmts)
{
    extraction_context cxt(fmts);
    reader             rdr(source);
    (void) rdr.next_token();

    allocation_counter allocations;
    auto               out  = cxt.extract<T>(rdr);
    const std::size_t  cost = allocations.count();

    ensure(out.has_value());
    return cost;
}

/// The same, for an extraction out of an in-memory \a source.
template <typename T>
std::size_t extraction_cost_of_value(const value& source, const formats& fmts)
{
    extraction_context cxt(fmts);
    reader             rdr = reader::from_value(source);
    (void) rdr.next_token();

    allocation_counter allocations;
    auto               out  = cxt.extract<T>(rdr);
    const std::size_t  cost = allocations.count();

    ensure(out.has_value());
    return cost;
}

/// The same `std::string`, extracted through an adapter left on the `value` bridge -- which is what every built-in
/// did before this. Both arms look the extractor up, run the same reader and hand back the same string; the only
/// difference between them is the `value` the bridge materialises to copy the bytes out of.
struct bridged_string
{
    std::string text;
};

class bridged_string_adapter final :
        public value_adapter_for<bridged_string>
{
protected:
    bridged_string create(extraction_context&, const value& from) const override
    {
        return bridged_string{ from.as_string() };
    }

    value to_json(const serialization_context&, const bridged_string& from) const override
    {
        return value(from.text);
    }
};

const formats& bridged_string_formats()
{
    static bridged_string_adapter instance;
    static const formats         out =
        [] ()
        {
            formats fmt = formats::compose({ formats::defaults() });
            fmt.register_adapter(&instance);
            return fmt;
        }();

    return out;
}

}

TEST(builtin_extraction_from_text_does_not_materialise_a_value)
{
    // This is the case's whole point: a built-in reads the token it is sitting on, so nothing is built to hold the
    // value on its way to the caller. Going through the bridge instead meant a `value` per leaf, and for a string
    // the bytes were copied into it and straight back out again.
    const formats fmts = formats::defaults();

    ensure_eq(0U, extraction_cost<std::int64_t>("1234", fmts));
    ensure_eq(0U, extraction_cost<double>("1234.5", fmts));
    ensure_eq(0U, extraction_cost<float>("1234.5", fmts));
    ensure_eq(0U, extraction_cost<bool>("true", fmts));
    ensure_eq(0U, extraction_cost<value>("1234", fmts));

    // Strings past every small-string buffer, so nothing hides inside one. The claim is relative rather than an exact
    // count: a debug standard library charges bookkeeping of its own per `std::string`, which both arms here pay and
    // which no absolute budget would survive. What the bridge pays on top of it is the `value` this no longer builds.
    const std::string_view canonical = R"("a string which is far too long for any small-string buffer")";
    const std::string_view escaped   = R"("a string\t far too long for any small-string buffer")";

    ensure_eq(0U, extraction_cost<std::string_view>(canonical, fmts));
    ensure_lt(extraction_cost<std::string>(canonical, fmts),
              extraction_cost<bridged_string>(canonical, bridged_string_formats())
             );
    ensure_lt(extraction_cost<std::string>(escaped, fmts),
              extraction_cost<bridged_string>(escaped, bridged_string_formats())
             );
}

TEST(builtin_coerce_reads_a_lent_tree_where_it_sits)
{
    // `coerce_string` on a tree the reader is lending encodes it in place. Rebuilding it through `read_value` first
    // would cost allocations in proportion to the tree -- around three per element for this one -- so a bound under
    // the element count is what separates the two, without pinning a number a debug allocator would move.
    constexpr std::size_t element_count = 200U;

    value tree = array();
    for (std::size_t idx = 0U; idx < element_count; ++idx)
        tree.push_back(std::string("element string long enough to be on the heap ") + std::to_string(idx));

    const formats coerce = formats::coerce();
    ensure_lt(extraction_cost_of_value<std::string>(tree, coerce), element_count);

    // Weighing one costs nothing at all: both openers carry their element count.
    ensure_eq(0U, extraction_cost_of_value<bool>(tree, coerce));
}

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Leaving the reader where the next thing expects it                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

/// Extract one \c T out of the middle of an array and report what the reader is sitting on afterwards. Every
/// extractor owes its caller the same thing -- one position past the value it read -- and a leaf which gets that
/// wrong shows up as a sibling being consumed twice or not at all.
template <typename T>
std::string_view token_after_extracting(std::string_view source, const formats& fmts = formats::defaults())
{
    extraction_context cxt(fmts);
    reader             rdr(source);

    (void) rdr.next_token();   // onto the `[`
    (void) rdr.next_token();   // onto the value to read

    auto out = cxt.extract<T>(rdr);
    ensure(out.has_value());
    ensure(rdr.good());
    return rdr.current().token_raw();
}

}

TEST(builtin_extractors_land_one_past_the_value)
{
    ensure_eq(std::string_view("2"), token_after_extracting<std::int32_t>("[ 1, 2 ]"));
    ensure_eq(std::string_view("2"), token_after_extracting<double>("[ 1.5, 2 ]"));
    ensure_eq(std::string_view("2"), token_after_extracting<float>("[ 1.5, 2 ]"));
    ensure_eq(std::string_view("2"), token_after_extracting<bool>("[ true, 2 ]"));
    ensure_eq(std::string_view("2"), token_after_extracting<std::string>(R"([ "a", 2 ])"));
    ensure_eq(std::string_view("2"), token_after_extracting<std::string>(R"([ "a\tb", 2 ])"));
    ensure_eq(std::string_view("2"), token_after_extracting<std::string_view>(R"([ "a", 2 ])"));
    ensure_eq(std::string_view("2"), token_after_extracting<value>("[ 1, 2 ]"));

    // A `value` steps over a whole subtree rather than into it.
    ensure_eq(std::string_view("2"), token_after_extracting<value>(R"([ { "a": [ 1 ] }, 2 ])"));

    const formats coerce = formats::coerce();
    ensure_eq(std::string_view("2"), token_after_extracting<std::int32_t>(R"([ "1", 2 ])", coerce));
    ensure_eq(std::string_view("2"), token_after_extracting<std::int32_t>("[ 1.5, 2 ]", coerce));
    ensure_eq(std::string_view("2"), token_after_extracting<double>(R"([ "1.5", 2 ])", coerce));
    ensure_eq(std::string_view("2"), token_after_extracting<bool>("[ 1, 2 ]", coerce));
    ensure_eq(std::string_view("2"), token_after_extracting<std::string>("[ 1, 2 ]", coerce));

    // The composites `coerce` answers without walking into them still have to be stepped over whole.
    ensure_eq(std::string_view("2"), token_after_extracting<bool>(R"([ { "a": [ 1 ] }, 2 ])", coerce));
    ensure_eq(std::string_view("2"), token_after_extracting<std::string>(R"([ { "a": [ 1 ] }, 2 ])", coerce));
}

}
