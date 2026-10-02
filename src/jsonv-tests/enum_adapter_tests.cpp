/// \file
/// Tests for \c enum_adapter, which reads a \c reader directly: a string from JSON text is looked up by its text, and
/// anything else is read as a \c value without moving the reader.
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
#include <jsonv/functional.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization/enum_adapter.hpp>
#include <jsonv/serialization_builder.hpp>
#include <jsonv/value.hpp>

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

enum class ring
{
    fire,
    wind,
    water,
    earth,
    heart,
};

/// Strings alone, as `serialization_builder_enum_strings` maps them, plus a second spelling of one ring.
const formats& ring_formats()
{
    static const formats instance =
        formats_builder()
            .enum_type<ring>("ring",
                             {
                               { ring::fire,  "fire"    },
                               { ring::wind,  "wind"    },
                               { ring::water, "water"   },
                               { ring::earth, "earth"   },
                               { ring::heart, "heart"   },
                               { ring::heart, "useless" },
                             }
                            )
            .register_container<std::vector<ring>>()
        .compose_checked(formats::defaults());

    return instance;
}

/// Case-insensitive, with more than one spelling of some rings -- and some of those spellings not strings at all.
const formats& ring_icase_formats()
{
    static const formats instance =
        formats_builder()
            .enum_type_icase<ring>("ring",
                                   {
                                     { ring::fire,  "fire"    },
                                     { ring::fire,  666       },
                                     { ring::wind,  "wind"    },
                                     { ring::water, "water"   },
                                     { ring::earth, "earth"   },
                                     { ring::earth, true      },
                                     { ring::heart, "heart"   },
                                     { ring::heart, "useless" },
                                   }
                                  )
            .register_container<std::vector<ring>>()
        .compose_checked(formats::defaults());

    return instance;
}

/// A mapping from every kind of JSON a mapping can be written in, and from a string past every small-string buffer.
const formats& mixed_formats()
{
    static const formats instance =
        formats_builder()
            .enum_type<ring>("ring",
                             {
                               { ring::fire,  "fire"                                                },
                               { ring::wind,  "a spelling far too long for any small-string buffer" },
                               { ring::water, 666                                                   },
                               { ring::earth, true                                                  },
                               { ring::heart, null                                                  },
                               { ring::fire,  array({ 1, 2 })                                       },
                             }
                            )
            .register_container<std::vector<ring>>()
        .compose_checked(formats::defaults());

    return instance;
}

/// Step a fresh reader off `document_start` and onto the value itself.
reader open(std::string_view source)
{
    reader out(source);
    (void) out.next_token();
    return out;
}

/// The same, over a tree rather than over text, so every case can be run against both sources. The two differ in what
/// the reader can lend, which is exactly what the adapter branches on.
reader open_value(const value& source)
{
    reader out = reader::from_value(source);
    (void) out.next_token();
    return out;
}

extract_options collecting()
{
    return extract_options::create_default()
                .failure_mode(extract_options::on_error::collect_all)
                .max_failures(10U);
}

bool mentions(const extraction_error::problem& problem, std::string_view text)
{
    return problem.message().find(text) != std::string::npos;
}

/// The \c ring extracted from \a source, read from the JSON text or from the \c value it parses to, which must succeed.
ring extracted(std::string_view source, bool from_value, const formats& fmts)
{
    const value        tree = parse(source);
    extraction_context cxt(fmts);
    auto               rdr = from_value ? open_value(tree) : open(source);

    auto out = cxt.extract<ring>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    return *out;
}

/// The single problem extracting a \c ring from \a source reports, read from the JSON text or from the \c value it
/// parses to.
extraction_error::problem refused(std::string_view source, bool from_value, const formats& fmts)
{
    const value        tree = parse(source);
    extraction_context cxt(fmts);
    auto               rdr = from_value ? open_value(tree) : open(source);

    auto out = cxt.extract<ring>(rdr);
    ensure(!out.has_value());
    ensure_eq(1U, cxt.problems().size());
    return cxt.problems().at(0);
}

/// What extracting a \c ring from \a source comes to: the ring, or the message it was refused with.
std::string outcome(std::string_view source, bool from_value, const formats& fmts)
{
    const value        tree = parse(source);
    extraction_context cxt(fmts);
    auto               rdr = from_value ? open_value(tree) : open(source);

    auto out = cxt.extract<ring>(rdr);
    if (out)
        return "ring " + std::to_string(static_cast<int>(*out));

    ensure_eq(1U, cxt.problems().size());
    return cxt.problems().at(0).message();
}

/// Extract a \c ring from the front of `[ element, 7 ]` and read what the reader is on afterwards. Every extractor owes
/// its caller one position past the value it read. Read as a value rather than viewed as a token, since a token from a
/// value-backed reader is a view of an arena which dies with that reader.
value after_extracting(std::string_view element, bool from_value, const formats& fmts)
{
    const std::string  text = "[ " + std::string(element) + ", 7 ]";
    const value        tree = parse(text);
    extraction_context cxt(fmts);
    reader             rdr = from_value ? reader::from_value(tree) : reader(std::string_view(text));

    (void) rdr.next_token();   // onto the `[`
    (void) rdr.next_token();   // onto the element

    ensure(cxt.extract<ring>(rdr).has_value());
    return read_value(rdr);
}

}

TEST(enum_adapter_reads_every_mapping_from_both_sources)
{
    // A string is looked up by its text and anything else by the `value` it reads as -- so `666.0` finds the mapping
    // for `666`, as `value::compare` has it. From a tree, the value is looked up where it sits.
    const std::pair<std::string_view, ring> cases[] =
        {
            { R"("fire")",                                                ring::fire  },
            { R"("a spelling far too long for any small-string buffer")", ring::wind  },
            { "666",                                                      ring::water },
            { "666.0",                                                    ring::water },
            { "true",                                                     ring::earth },
            { "null",                                                     ring::heart },
            { "[ 1, 2 ]",                                                 ring::fire  },
        };

    for (const auto& [text, expected] : cases)
        for (bool from_value : { false, true })
            ensure(expected == extracted(text, from_value, mixed_formats()));
}

TEST(enum_adapter_reads_an_escaped_string_as_it_decodes)
{
    // An escaped string has no contiguous source form to compare against, so it is decoded first -- and from then on it
    // is the string it decodes to, under either ordering.
    for (std::string_view text : { R"("fi\u0072e")", R"("\u0066ire")" })
    {
        ensure(open(text).current_type() == ast_node_type::string_escaped);
        ensure(ring::fire == extracted(text, false, ring_formats()));
    }

    ensure(ring::fire == extracted(R"("F\u0049RE")", false, ring_icase_formats()));
    ensure(ring::heart == extracted(R"("USE\u004cESS")", false, ring_icase_formats()));
}

TEST(enum_adapter_icase_multimapping_matches_every_spelling)
{
    // Several spellings of one ring, some of them not strings at all, and each string in any case.
    const std::pair<std::string_view, ring> cases[] =
        {
            { R"("FIRE")",    ring::fire  },
            { R"("fIrE")",    ring::fire  },
            { "666",          ring::fire  },
            { R"("Earth")",   ring::earth },
            { "true",         ring::earth },
            { R"("heart")",   ring::heart },
            { R"("USELESS")", ring::heart },
        };

    for (const auto& [text, expected] : cases)
        for (bool from_value : { false, true })
            ensure(expected == extracted(text, from_value, ring_icase_formats()));
}

TEST(enum_adapter_lands_one_past_the_value)
{
    for (bool from_value : { false, true })
    {
        ensure_eq(value(7), after_extracting(R"("fire")",      from_value, mixed_formats()));
        ensure_eq(value(7), after_extracting(R"("\u0066ire")", from_value, ring_formats()));
        ensure_eq(value(7), after_extracting("666",            from_value, mixed_formats()));
        ensure_eq(value(7), after_extracting("true",           from_value, mixed_formats()));
        ensure_eq(value(7), after_extracting("null",           from_value, mixed_formats()));
        ensure_eq(value(7), after_extracting("[ 1, 2 ]",       from_value, mixed_formats()));
    }
}

TEST(enum_adapter_miss_lists_what_it_would_accept)
{
    // The value as JSON, then everything the mapping accepts in the mapping's own order -- which puts the kinds in the
    // order `value::compare` does, and lists every spelling of a ring.
    const std::string rings = R"((expected one of "earth", "fire", "heart", "useless", "water", "wind"))";

    for (bool from_value : { false, true })
    {
        ensure_eq(R"(Invalid value for ring: "bogus" )" + rings,
                  refused(R"("bogus")", from_value, ring_formats()).message()
                 );
        ensure_eq(R"(Invalid value for ring: 7 (expected one of true, 666, )"
                  R"("earth", "fire", "heart", "useless", "water", "wind"))",
                  refused("7", from_value, ring_icase_formats()).message()
                 );

        // Text written with escapes is described as the string it decodes to.
        ensure_eq(R"(Invalid value for ring: "bogus" )" + rings,
                  refused(R"("b\u006fgus")", from_value, ring_formats()).message()
                 );
    }

    // With nothing to accept, there is nothing to list.
    const formats nothing = formats_builder()
                                .enum_type<ring>("ring", {})
                            .compose_checked(formats::defaults());
    ensure_eq(std::string(R"(Invalid value for ring: "fire")"), refused(R"("fire")", false, nothing).message());
}

TEST(enum_adapter_miss_is_placed_where_the_context_says)
{
    const value bogus("bogus");

    for (bool from_value : { false, true })
    {
        // With nothing else to say where it is, the reader does -- which the `value` bridge did not consult, so a miss
        // in the middle of a document used to be reported at the root.
        {
            const std::string_view text = R"({ "a": [ "fire", "bogus" ] })";
            const value            tree = parse(text);

            extraction_context cxt(ring_formats());
            reader             rdr = from_value ? reader::from_value(tree) : reader(text);
            for (int step = 0; step < 5; ++step)   // onto the `{`, "a", the `[`, "fire" and then "bogus"
                (void) rdr.next_token();

            ensure(!cxt.extract<ring>(rdr).has_value());
            ensure_eq(1U, cxt.problems().size());
            ensure_eq(path::create(".a[1]"), cxt.problems().at(0).path());

            // And the cursor is still on the value, for whatever recovers from it.
            ensure_eq(bogus, read_value(rdr));
        }

        // A base path or a scope outranks the reader.
        {
            extraction_context cxt(ring_formats(), std::nullopt, path::create(".payload"));
            auto               rdr = from_value ? open_value(bogus) : open(R"("bogus")");

            ensure(!cxt.extract<ring>(rdr).has_value());
            ensure_eq(path::create(".payload"), cxt.problems().at(0).path());
        }
        {
            extraction_context             cxt(ring_formats());
            extraction_context::path_scope named(cxt, std::string_view("renamed"));
            auto                           rdr = from_value ? open_value(bogus) : open(R"("bogus")");

            ensure(!cxt.extract<ring>(rdr).has_value());
            ensure_eq(path::create(".renamed"), cxt.problems().at(0).path());
        }
    }
}

TEST(enum_adapter_collect_all_resumes_after_a_miss)
{
    // A miss is refused with the cursor still on it, so the container steps over it and no further -- over the whole
    // of a structure, too. Stepping one too far would drop the ring after it, or the problem with the next one.
    const std::string_view text = R"([ "bogus", "fire", { "a": 1 }, [ 1, 3 ], "nope" ])";
    const value            tree = parse(text);

    for (bool from_value : { false, true })
    {
        extraction_context cxt(ring_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
        auto               rdr = from_value ? open_value(tree) : open(text);

        ensure(!cxt.extract<std::vector<ring>>(rdr).has_value());
        ensure_eq(4U, cxt.problems().size());
        ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
        ensure_eq(path::create("[2]"), cxt.problems().at(1).path());
        ensure_eq(path::create("[3]"), cxt.problems().at(2).path());
        ensure_eq(path::create("[4]"), cxt.problems().at(3).path());
        ensure(mentions(cxt.problems().at(1), R"(Invalid value for ring: {"a":1})"));
        ensure(mentions(cxt.problems().at(3), R"(Invalid value for ring: "nope")"));
    }
}

TEST(enum_adapter_collect_all_resumes_after_a_value_which_cannot_be_read)
{
    // An escape which does not decode and a number with no `double` fail before anything is looked up, with the cursor
    // still on them -- so the container steps over each, and over the whole of an array it could not read.
    const std::string_view text = R"([ "\uD800", "fire", 1e400, [ 1e400 ], "nope" ])";

    extraction_context cxt(ring_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
    auto               rdr = open(text);

    ensure(!cxt.extract<std::vector<ring>>(rdr).has_value());
    ensure_eq(4U, cxt.problems().size());
    ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
    ensure_eq(path::create("[2]"), cxt.problems().at(1).path());
    ensure_eq(path::create("[3]"), cxt.problems().at(2).path());
    ensure_eq(path::create("[4]"), cxt.problems().at(3).path());
    ensure(mentions(cxt.problems().at(3), R"(Invalid value for ring: "nope")"));
}

TEST(enum_adapter_text_and_value_sources_agree)
{
    // Text is looked up by where its string would sort, and a lent value by the table's own ordering, so the two
    // sources give independent answers to the same question. The keys sit at the edges of a string ordering: empty, a
    // prefix of another, differing only in case -- one spelling the case-insensitive table drops, since it keeps the
    // first -- holding a NUL, and past ASCII.
    const std::initializer_list<std::pair<ring, value>> mapping =
        {
            { ring::fire,  "fire"                         },
            { ring::wind,  "Fire"                         },
            { ring::water, ""                             },
            { ring::earth, value(std::string("a\0b", 3U)) },
            { ring::heart, "\xC3\xA9"                     },
        };

    const formats sensitive = formats_builder()
                                  .register_adapter(std::make_shared<enum_adapter<ring>>("ring", mapping))
                              .compose_checked(formats::defaults());
    const formats insensitive = formats_builder()
                                    .register_adapter(std::make_shared<enum_adapter_icase<ring>>("ring", mapping))
                                .compose_checked(formats::defaults());

    for (std::string_view text : { R"("")",
                                   R"("fire")",
                                   R"("Fire")",
                                   R"("FIRE")",
                                   R"("fir")",
                                   R"("fires")",
                                   R"("firf")",
                                   R"("a")",
                                   R"("a\u0000")",
                                   R"("a\u0000b")",
                                   R"("a\u0000c")",
                                   R"("\u0000")",
                                   "\"\xC3\xA9\"",
                                   "\"\xC3\x89\"",
                                   R"("\u00e9")",
                                   R"("\u00c9")",
                                   "\"\xF0\x9F\x98\x80\"",
                                   R"("~")",
                                   "0",
                                   "true",
                                   "null",
                                   "[]",
                                   "{}",
                                 }
        )
    {
        for (const formats* fmts : { &sensitive, &insensitive })
            ensure_eq(outcome(text, false, *fmts), outcome(text, true, *fmts));
    }
}

TEST(enum_adapter_other_orderings_are_used_as_given)
{
    // An ordering the library does not know can only be asked about values, so text is read into one first -- and the
    // table keeps that ordering, which is the order a miss lists it in. `std::less<>` is transparent, but it cannot place
    // text before a value, so it is one of these too.
    const std::initializer_list<std::pair<ring, value>> mapping =
        {
            { ring::fire,  "fire"    },
            { ring::wind,  "wind"    },
            { ring::water, "water"   },
            { ring::earth, "earth"   },
            { ring::heart, "heart"   },
            { ring::heart, "useless" },
        };

    using greater_adapter     = enum_adapter<ring, std::less<ring>, value_greater>;
    using transparent_adapter = enum_adapter<ring, std::less<ring>, std::less<>>;

    const formats greater = formats_builder()
                                .register_adapter(std::make_shared<greater_adapter>("ring", mapping))
                            .compose_checked(formats::defaults());
    const formats transparent = formats_builder()
                                    .register_adapter(std::make_shared<transparent_adapter>("ring", mapping))
                                .compose_checked(formats::defaults());

    for (bool from_value : { false, true })
    {
        ensure(ring::heart == extracted(R"("useless")",    from_value, greater));
        ensure(ring::heart == extracted(R"("useless")",    from_value, transparent));
        ensure(ring::fire  == extracted(R"("\u0066ire")", from_value, transparent));

        ensure_eq(std::string(R"(Invalid value for ring: "bogus" )"
                              R"((expected one of "wind", "water", "useless", "heart", "fire", "earth"))"
                             ),
                  refused(R"("bogus")", from_value, greater).message()
                 );
        ensure_eq(std::string(R"(Invalid value for ring: "bogus" )"
                              R"((expected one of "earth", "fire", "heart", "useless", "water", "wind"))"
                             ),
                  refused(R"("bogus")", from_value, transparent).message()
                 );
    }
}

namespace
{

/// An enumerator which refuses to be copied, or to be moved, while \c refusing says so -- and only as the value 9, so
/// that building the mapping and extracting any other value go through.
struct touchy
{
    enum class refusal
    {
        none,
        copy,
        move,
    };

    static inline refusal refusing = refusal::none;

    int raw = 0;

    touchy() = default;

    explicit touchy(int raw) :
            raw(raw)
    { }

    touchy(const touchy& other) :
            raw(other.raw)
    {
        if (refusing == refusal::copy && raw == 9)
            throw std::runtime_error("copy refuses");
    }

    touchy(touchy&& other) :
            raw(other.raw)
    {
        if (refusing == refusal::move && raw == 9)
            throw std::runtime_error("move refuses");
    }

    touchy& operator=(const touchy&) = default;
    touchy& operator=(touchy&&)      = default;

    friend bool operator<(const touchy& a, const touchy& b)
    {
        return a.raw < b.raw;
    }
};

/// Sets \c touchy::refusing for as long as it lives, so a failing test cannot leave it set for the next one.
class refusing_while
{
public:
    explicit refusing_while(touchy::refusal what)
    {
        touchy::refusing = what;
    }

    refusing_while(const refusing_while&)            = delete;
    refusing_while& operator=(const refusing_while&) = delete;

    ~refusing_while()
    {
        touchy::refusing = touchy::refusal::none;
    }
};

const formats& touchy_formats()
{
    static const formats instance =
        formats_builder()
            .enum_type<touchy>("touchy",
                               {
                                 { touchy(1), "one"  },
                                 { touchy(9), "nine" },
                               }
                              )
            .register_container<std::vector<touchy>>()
        .compose_checked(formats::defaults());

    return instance;
}

}

TEST(enum_adapter_copy_or_move_failure_keeps_the_following_diagnostics)
{
    // The enumerator is copied out of the table while the cursor is still on its value, and moved out once the cursor
    // is past it. Either can fail, and whatever recovers has to step over the value exactly once: twice loses the
    // problem with the next element, and at the end of the array invents an `Unterminated array`.
    for (auto refusal : { touchy::refusal::copy, touchy::refusal::move })
    {
        for (bool from_value : { false, true })
        {
            {
                const std::string_view text = R"([ "nine", "bogus" ])";
                const value            tree = parse(text);
                extraction_context     cxt(touchy_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
                auto                   rdr = from_value ? open_value(tree) : open(text);

                refusing_while refusing(refusal);
                ensure(!cxt.extract<std::vector<touchy>>(rdr).has_value());
                ensure_eq(2U, cxt.problems().size());
                ensure_eq(path::create("[0]"), cxt.problems().at(0).path());
                ensure(mentions(cxt.problems().at(0), "refuses"));
                ensure_eq(path::create("[1]"), cxt.problems().at(1).path());
                ensure(mentions(cxt.problems().at(1), R"(Invalid value for touchy: "bogus")"));
            }

            {
                const std::string_view text = R"([ "one", "nine" ])";
                const value            tree = parse(text);
                extraction_context     cxt(touchy_formats(), std::nullopt, jsonv::path(), nullptr, collecting());
                auto                   rdr = from_value ? open_value(tree) : open(text);

                refusing_while refusing(refusal);
                ensure(!cxt.extract<std::vector<touchy>>(rdr).has_value());
                ensure_eq(1U, cxt.problems().size());
                ensure_eq(path::create("[1]"), cxt.problems().at(0).path());
                ensure(mentions(cxt.problems().at(0), "refuses"));
            }
        }
    }
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

}

TEST(enum_adapter_from_text_builds_nothing)
{
    // The whole point of reading the reader: a string is looked up by its text and a number where it sits, so nothing
    // is built on the way to the ring. On the `value` bridge every one of these built a `value` from text, and the
    // long string paid for a copy of its text besides.
    ensure_eq(0U, extraction_cost<ring>(R"("fire")", ring_formats()));
    ensure_eq(0U, extraction_cost<ring>(R"("a spelling far too long for any small-string buffer")", mixed_formats()));
    ensure_eq(0U, extraction_cost<ring>(R"("FIRE")", ring_icase_formats()));
    ensure_eq(0U, extraction_cost<ring>("666", mixed_formats()));
    ensure_eq(0U, extraction_cost<ring>("true", mixed_formats()));
    ensure_eq(0U, extraction_cost_of_value<ring>(value("fire"), ring_formats()));

    // An escaped string has to be decoded before it can be compared, and decoding is all it costs: no more than
    // extracting the string itself. Relative rather than absolute, since a debug standard library charges for a
    // `std::string` in ways no fixed budget survives.
    const std::string_view escaped = R"("a spelling far too long for any small-string buffe\u0072")";
    ensure_le(extraction_cost<ring>(escaped, mixed_formats()), extraction_cost<std::string>(escaped, mixed_formats()));
}

#endif

}
