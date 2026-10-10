/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"
#include "filesystem_util.hpp"

#include <jsonv/ast.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/parse_index.hpp>

#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

template <typename TNode>
TNode parse_single(std::string_view            src,
                   std::string_view            expected,
                   const jsonv::parse_options& options = jsonv::parse_options()
                  )
{
    auto ast = jsonv::parse_index::parse(src, options);
    ensure_eq(to_string(ast), expected);

    auto iter = ast.begin();
    ensure_eq(jsonv::ast_node_type::document_start, (*iter).type());

    ++iter;
    auto result = (*iter).as<TNode>();

    return result;
}

TEST(ast_parse_literal_true)
{
    parse_single<jsonv::ast_node::literal_true>("true", "^t$");
}

TEST(ast_parse_literal_true_incomplete)
{
    parse_single<jsonv::ast_node::error>("tru", "^!");
}

TEST(ast_parse_literal_false)
{
    parse_single<jsonv::ast_node::literal_false>(" false", "^f$");
}

TEST(ast_parse_literal_null)
{
    parse_single<jsonv::ast_node::literal_null>("null ", "^n$");
}

TEST(ast_parse_integers)
{
    auto node_0 = parse_single<jsonv::ast_node::integer>("0", "^i$");
    ensure_eq(0, node_0.value());

    auto node_1 = parse_single<jsonv::ast_node::integer>(" 1", "^i$");
    ensure_eq(1, node_1.value());

    auto node_8 = parse_single<jsonv::ast_node::integer>("12345678 ", "^i$");
    ensure_eq(12345678, node_8.value());
}

TEST(ast_parse_integer_only_minus)
{
    parse_single<jsonv::ast_node::error>("-", "^!");
}

TEST(ast_parse_string_canonical)
{
    auto node = parse_single<jsonv::ast_node::string_canonical>("\"1234567890\"", "^s$");
    ensure_eq(12U, node.token_size());
    ensure_eq(10U, node.value().size());
}

TEST(ast_parse_string_empty)
{
    auto node = parse_single<jsonv::ast_node::string_canonical>("\"\"", "^s$");
    ensure_eq(2U, node.token_size());
    ensure_eq(0U, node.value().size());
}

template <std::size_t N>
std::size_t sstrlen(const char (&)[N])
{
    return N-1;
}

TEST(ast_parse_string_double_reverse_solidus_before_escaped_quote)
{
    static const char tokens[] = R"("\\\" and keep going")";

    auto node = parse_single<jsonv::ast_node::string_escaped>(tokens, "^S$");
    ensure_eq(sstrlen(tokens), node.token_size());
}

TEST(ast_parse_nothing)
{
    auto ast = jsonv::parse_index::parse("  ");
    ensure_eq(to_string(ast), "^$");
}

TEST(ast_parse_comment)
{
    parse_single<jsonv::ast_node::literal_null>("null /* <- still null */ ",
                                                "^n$",
                                                jsonv::parse_options().comments(true)
                                               );
}

TEST(ast_empty_array)
{
    auto ast = jsonv::parse_index::parse("[ ]");
    ensure_eq(to_string(ast), "^[]$");
}

TEST(ast_array_elems)
{
    auto ast = jsonv::parse_index::parse("[ 1, 2,\t 3, \"Bob\\n\"]");
    ensure_eq(to_string(ast), "^[iiiS]$");

    auto iter = ast.begin();
    ensure_eq(jsonv::ast_node_type::document_start, (*iter).type());
    ++iter;
    auto array_node = (*iter).as<jsonv::ast_node::array_begin>();
    ensure_eq(4U, array_node.element_count());
}

// `end()` is `&data(data_size)` and `operator++` decodes the entry it lands on, so that slot has to be readable.
// Sweeping the requested capacity guarantees one run ends with `data_size == data_capacity`, which is where the slot
// used to sit one past the allocation.
TEST(ast_iterate_to_end_across_buffer_capacities)
{
    const std::string_view src = "[ 1, 2,\t 3, \"Bob\\n\"]";

    for (std::size_t capacity = 0U; capacity <= 64U; ++capacity)
    {
        auto ast   = jsonv::parse_index::parse(src, capacity);
        auto count = 0U;

        for (auto iter = ast.begin(); iter != ast.end(); ++iter)
            ++count;

        ensure_eq(8U, count);
    }
}

// The buffer's size in bytes is computed from the requested capacity. One too large for that to fit in a `size_t` used
// to wrap to a small allocation which still claimed the whole capacity, so the first write ran off its end -- and for
// `max / 8 - 1`, the allocation was smaller than the header itself.
TEST(ast_parse_rejects_unrepresentable_buffer_capacity)
{
    constexpr auto max = std::numeric_limits<std::size_t>::max();

    for (std::size_t capacity : { max, max / 8U + 1U, max / 8U, max / 8U - 1U })
        ensure_throws(std::length_error, jsonv::parse_index::parse("[]", capacity));
}

TEST(ast_object_empty)
{
    auto ast = jsonv::parse_index::parse("\t{}\t");
    ensure_eq(to_string(ast), "^{}$");

    auto iter = ast.begin();
    ensure_eq(jsonv::ast_node_type::document_start, (*iter).type());
    ++iter;
    auto object_node = (*iter).as<jsonv::ast_node::object_begin>();
    ensure_eq(0U, object_node.element_count());
}

TEST(ast_object)
{
    auto ast = jsonv::parse_index::parse(R"( { "a": 1.0, "b": "Bob", "c": [], "d\t": {} } )");
    ensure_eq(to_string(ast), "^{kdksk[]K{}}$");
}

/// An object's first member is read by the `{` case rather than by the `,` case, and only the latter recorded that a
/// value was owed. `{"a":}` therefore parsed as a *successful* index whose tape is a key and nothing else -- which
/// `jsonv::parse` still rejected, since building the tree walks onto the `}` where a value should be, but anything
/// reading the index or a `reader` directly was told the document was fine.
TEST(ast_parse_object_missing_first_member_value)
{
    for (std::string_view src : { R"({"a":})", R"({ "a" : })", R"([{"a":}])", R"({"a":{"b":}})" })
    {
        auto ast = jsonv::parse_index::parse(src);
        ensure(!ast.success());
    }

    // The same shape in any position but the first already failed, and still does.
    ensure(!jsonv::parse_index::parse(R"({"a":1,"b":})").success());

    // An empty object reaches the very same code path -- `get_key` stops on the `}` without consuming anything --
    // and must keep parsing.
    ensure(jsonv::parse_index::parse("{}").success());
    ensure(jsonv::parse_index::parse("{ }").success());
    ensure(jsonv::parse_index::parse(R"({"a":1})").success());
    ensure(jsonv::parse_index::parse(R"({"a":{}})").success());
    ensure(jsonv::parse_index::parse(R"([{},{"a":1}])").success());
}

TEST(ast_parse_object_with_numeric_keys)
{
    auto ast = jsonv::parse_index::parse("{ 3: \"Bob\", \"a\": \"A\" }");
    ensure(!ast.success());
    ensure_eq(to_string(ast), "^{!");
}

/// A document which runs out while an array or object is still open has ended early: nothing in it closed the wrong
/// structure. The parser closed the document before looking at what was still open, and found that structure where
/// the document's opener should be, so `[1, 2` reported `mismatched_close` -- at the end of an input with no closing
/// character in it -- while `{` reported `unexpected_eof`. The tape kept a `document_end` for a document which never
/// ended, too.
///
/// \see https://github.com/tgockel/json-voorhees/issues/262
TEST(ast_parse_document_ending_inside_a_structure_is_unexpected_eof)
{
    struct
    {
        std::string_view src;
        std::string_view tape;
        jsonv::ast_error code;
        std::size_t      at;
    }
    const cases[] =
    {
        { "[",          "^[!",    jsonv::ast_error::unexpected_eof,   1U },
        { "[1, 2",      "^[ii!",  jsonv::ast_error::unexpected_eof,   5U },
        { "[1, 2 ",     "^[ii!",  jsonv::ast_error::unexpected_eof,   6U },
        { "[1,",        "^[i!",   jsonv::ast_error::unexpected_eof,   3U },
        { R"({"a": 1)", "^{ki!",  jsonv::ast_error::unexpected_eof,   7U },
        { R"({"a":)",   "^{k!",   jsonv::ast_error::unexpected_eof,   5U },
        { "[[1]",       "^[[i]!", jsonv::ast_error::unexpected_eof,   4U },
        { R"({"a": [)", "^{k[!",  jsonv::ast_error::unexpected_eof,   7U },
        // `{` alone runs out of input while looking for a key, which always reported this.
        { "{",          "^{!",    jsonv::ast_error::unexpected_eof,   1U },
        // A close which really does not match its opener keeps its code.
        { "[1}",        "^[i}!",  jsonv::ast_error::mismatched_close, 2U },
        { "5]",         "^i]!",   jsonv::ast_error::mismatched_close, 1U },
    };

    for (const auto& c : cases)
    {
        auto ast = jsonv::parse_index::parse(c.src);
        ensure_eq(c.tape, to_string(ast));

        try
        {
            ast.validate();
            ensure(!"parse_error was not thrown");
        }
        catch (const jsonv::parse_error& ex)
        {
            ensure_eq(c.at, ex.character().value());
            ensure(std::string_view(ex.what()).ends_with(to_string(c.code)));
        }
    }
}

/// Only the `,`, `]` and `}` cases asked whether they were allowed where they were, so a value was taken wherever it
/// appeared. Two in a row with nothing between them parsed: `[1 2]` as an array whose `element_count()` was 1, since
/// that counts commas, and `5 6` as a document holding two values, which `jsonv::parse` then failed to build with
/// `std::invalid_argument`. A top-level `,` passed as well, so `5,` parsed as `5`.
///
/// \see https://github.com/tgockel/json-voorhees/issues/261
TEST(ast_parse_values_without_a_comma_between_them_are_refused)
{
    struct
    {
        std::string_view src;
        std::string_view tape;
        jsonv::ast_error code;
        std::size_t      at;
        bool             comments = false;
    }
    const cases[] =
    {
        // Inside a structure the `,` is missing. Every kind of value reports it.
        { "[1 2]",              "^[i!",   jsonv::ast_error::expected_comma,  3U },
        { "[1, 2 3]",           "^[ii!",  jsonv::ast_error::expected_comma,  6U },
        { "[1 -2]",             "^[i!",   jsonv::ast_error::expected_comma,  3U },
        { R"(["a" "b"])",       "^[s!",   jsonv::ast_error::expected_comma,  5U },
        { "[true false]",       "^[t!",   jsonv::ast_error::expected_comma,  6U },
        { "[false null]",       "^[f!",   jsonv::ast_error::expected_comma,  7U },
        { "[null true]",        "^[n!",   jsonv::ast_error::expected_comma,  6U },
        { "[[] []]",            "^[[]!",  jsonv::ast_error::expected_comma,  4U },
        { "[[] {}]",            "^[[]!",  jsonv::ast_error::expected_comma,  4U },
        { "[{} 1]",             "^[{}!",  jsonv::ast_error::expected_comma,  4U },
        // An object read the `"b"` as a string value, and only failed at the `:` after it, with "unexpected token".
        { R"({"a": 1 "b": 2})", "^{ki!",  jsonv::ast_error::expected_comma,  8U },
        { R"({"a": [] {}})",    "^{k[]!", jsonv::ast_error::expected_comma,  9U },
        { "[1 /* c */ 2]",      "^[i!",   jsonv::ast_error::expected_comma, 11U, true },
        // Directly inside the document, where a second value is trailing input.
        { "5 6",                "^i!",    jsonv::ast_error::expected_eof,    2U },
        { R"("a" "b")",         "^s!",    jsonv::ast_error::expected_eof,    4U },
        { "5 [1]",              "^i!",    jsonv::ast_error::expected_eof,    2U },
        { "5 {}",               "^i!",    jsonv::ast_error::expected_eof,    2U },
        { "true false",         "^t!",    jsonv::ast_error::expected_eof,    5U },
        { "5,",                 "^i!",    jsonv::ast_error::expected_eof,    1U },
        { "5, 6",               "^i!",    jsonv::ast_error::expected_eof,    1U },
        { "5 /* c */ 6",        "^i!",    jsonv::ast_error::expected_eof,   10U, true },
        // A top-level structure was always followed by a check for the end of the input, and says the same thing.
        { "[1] 2",              "^[i]!",  jsonv::ast_error::expected_eof,    4U },
        { "[1],",               "^[i]!",  jsonv::ast_error::expected_eof,    3U },
    };

    for (const auto& c : cases)
    {
        const auto options = jsonv::parse_options().comments(c.comments);

        auto ast = jsonv::parse_index::parse(c.src, options);
        ensure_eq(c.tape, to_string(ast));

        try
        {
            ast.validate();
            ensure(!"parse_error was not thrown");
        }
        catch (const jsonv::parse_error& ex)
        {
            ensure_eq(c.at, ex.character().value());
            ensure(std::string_view(ex.what()).ends_with(to_string(c.code)));
        }

        // Neither success nor the `std::invalid_argument` from building a tree out of a tape with two values on it.
        ensure_throws(jsonv::parse_error, jsonv::parse(c.src, options));
    }
}

TEST(ast_parse_values_with_a_comma_between_them_still_parse)
{
    using jsonv::array;

    const auto with_comments = jsonv::parse_options().comments(true);

    ensure_eq(array({ 1, 2 }),             jsonv::parse("[1, 2]"));
    ensure_eq(array({ 1, -2 }),            jsonv::parse("[1,-2]"));
    ensure_eq(array({ array(), array() }), jsonv::parse("[[], []]"));
    ensure_eq(jsonv::value(5),             jsonv::parse("5"));
    ensure_eq(jsonv::value(5),             jsonv::parse(" 5 "));
    ensure_eq(jsonv::value(5),             jsonv::parse("5 /* c */", with_comments));
    ensure_eq(array({ 1, 2 }),             jsonv::parse("[1 /* c */, 2]", with_comments));
}

/// The main loop and the check for trailing input after a top-level structure both stopped at a NUL as though the input
/// ended there, so everything after one was dropped unread: `{"a":1}\0{"b":2}` parsed as `{"a":1}` and `0\0garbage` as
/// `0`, under `create_strict()` as well. Only `end` says where the input stops, and a NUL is a byte like any other
/// which cannot begin a value.
///
/// \see https://github.com/tgockel/json-voorhees/issues/279
TEST(ast_parse_nul_outside_a_string_is_refused)
{
    struct
    {
        std::string_view src;
        std::string_view tape;
        jsonv::ast_error code;
        std::size_t      at;
    }
    const cases[] =
    {
        { std::string_view("0\0garbage", 9U),            "^i!",    jsonv::ast_error::unexpected_token, 1U },
        // After a top-level structure, the NUL is trailing input, as the `2` of `[1] 2` is.
        { std::string_view("[1]\0garbage", 11U),         "^[i]!",  jsonv::ast_error::expected_eof,     3U },
        { std::string_view("{\"a\":1}\0{\"b\":2}", 15U), "^{ki}!", jsonv::ast_error::expected_eof,     7U },
        // A view which takes in the terminator of the C string it was made from.
        { std::string_view("[1]", 4U),                   "^[i]!",  jsonv::ast_error::expected_eof,     3U },
        // Inside a structure, the loop used to end with it still open, which was reported as a truncated document.
        { std::string_view("[1\0]", 4U),                 "^[i!",   jsonv::ast_error::unexpected_token, 2U },
        { std::string_view("\0", 1U),                    "^!",     jsonv::ast_error::unexpected_token, 0U },
    };

    for (const auto& options : { jsonv::parse_options(), jsonv::parse_options::create_strict() })
    {
        for (const auto& c : cases)
        {
            auto ast = jsonv::parse_index::parse(c.src, options);
            ensure_eq(c.tape, to_string(ast));

            try
            {
                ast.validate();
                ensure(!"parse_error was not thrown");
            }
            catch (const jsonv::parse_error& ex)
            {
                ensure_eq(c.at, ex.character().value());
                ensure(std::string_view(ex.what()).ends_with(to_string(c.code)));
            }

            ensure_throws(jsonv::parse_error, jsonv::parse(c.src, options));
        }

        // An escaped NUL is part of a string, and still parses.
        ensure_eq(jsonv::array({ std::string(1, '\0') }), jsonv::parse(R"(["\u0000"])", options));
    }
}

/// An opener reserves slots for its matching close token and its element count, but neither is known until that close
/// token arrives. A failed parse must still leave them determinate, because `iterator::operator*` reads the element
/// count unconditionally when it builds an `object_begin` or `array_begin`.
///
/// \see https://github.com/tgockel/json-voorhees/issues/150
TEST(ast_parse_failed_parse_has_determinate_element_count)
{
    // Dirty the heap so a fresh allocation is unlikely to come back zeroed -- without this, reading an uninitialized
    // slot would usually happen to return 0 and the test would pass either way.
    {
        std::vector<std::vector<std::uint64_t>> dirt;
        for (std::size_t n = 0; n < 64U; ++n)
            dirt.emplace_back(1024U, 0xdeadbeefcafef00dULL);
    }

    struct
    {
        std::string_view src;
        std::size_t      expected_count;
    }
    const cases[] =
    {
        // Structures which never close at all.
        { "{",          0U },
        { R"({ "a": 1)", 1U },
        { "[",          0U },
        { "[ 1, 2",     2U },
        // Structures which close correctly, but where trailing input is rejected from inside the close handling --
        // before the opener's slots have been written.
        { "[]x",        0U },
        { "{}x",        0U },
        { "[ 1, 2 ]x",  2U },
        { R"({ "a": 1 }x)", 1U },
    };

    for (const auto& c : cases)
    {
        auto ast = jsonv::parse_index::parse(c.src);
        ensure(!ast.success());

        auto iter = ast.begin();
        ++iter;     // document_start -> the opener

        auto node  = *iter;
        auto count = node.type() == jsonv::ast_node_type::object_begin
                   ? node.as<jsonv::ast_node::object_begin>().element_count()
                   : node.as<jsonv::ast_node::array_begin>().element_count();
        ensure_eq(c.expected_count, count);
    }
}

/// Stepping over a structure on a failed parse must stay inside the tape. A structure which closed but never had its
/// end recorded would otherwise jump by an uninitialized displacement.
///
/// \see https://github.com/tgockel/json-voorhees/issues/150
TEST(ast_skip_subtree_on_failed_parse_stays_in_the_tape)
{
    {
        std::vector<std::vector<std::uint64_t>> dirt;
        for (std::size_t n = 0; n < 64U; ++n)
            dirt.emplace_back(1024U, 0xdeadbeefcafef00dULL);
    }

    struct
    {
        std::string_view src;
        bool             root_closed;
    }
    const cases[] =
    {
        // The root structure is properly closed; only the trailing input is bad. Stepping over it lands on the error,
        // because the structure really does end at its own close token.
        { "[]x",             true  },
        { "{}x",             true  },
        { "[ 1, 2 ]x",       true  },
        { R"({ "a": 1 }x)",  true  },
        // The root structure never closed, so it ends at the error itself, and stepping past that reaches the end.
        { "{",               false },
        { "[ 1, 2",          false },
        { R"({ "a": 1)",     false },
    };

    for (const auto& c : cases)
    {
        auto ast = jsonv::parse_index::parse(c.src);
        ensure(!ast.success());

        auto root = ast.begin();
        ++root;                     // document_start -> the opener

        auto after_root = root;
        after_root.skip_subtree();

        // Whatever the shape of the failure, the jump has to stay within the tape.
        ensure(after_root <= ast.end());

        if (c.root_closed)
        {
            ensure(after_root != ast.end());
            ensure_eq(jsonv::ast_node_type::error, (*after_root).type());
        }
        else
        {
            ensure(after_root == ast.end());
        }

        // The document never closes on a failed parse, so stepping over it always reaches the end.
        auto after_document = ast.begin();
        ensure(after_document.skip_subtree() == ast.end());
    }
}

/// Step over the structure at \a pos the slow way, by counting openers and closers. This is what `skip_subtree`
/// replaces, and it is the oracle it is checked against.
static jsonv::parse_index::iterator walked_skip_subtree(const jsonv::parse_index&    index,
                                                        jsonv::parse_index::iterator pos
                                                       )
{
    std::size_t depth = 0U;

    do
    {
        switch ((*pos).type())
        {
        case jsonv::ast_node_type::document_start:
        case jsonv::ast_node_type::object_begin:
        case jsonv::ast_node_type::array_begin:
            ++depth;
            break;
        case jsonv::ast_node_type::document_end:
        case jsonv::ast_node_type::object_end:
        case jsonv::ast_node_type::array_end:
            --depth;
            break;
        default:
            break;
        }

        ++pos;
    } while (depth > 0U && pos != index.end());

    return pos;
}

static void ensure_skip_subtree_matches_walking(std::string_view src)
{
    auto ast = jsonv::parse_index::parse(src);
    ensure(ast.success());

    std::size_t structures_checked = 0U;

    for (auto iter = ast.begin(); iter != ast.end(); ++iter)
    {
        switch ((*iter).type())
        {
        case jsonv::ast_node_type::document_start:
        case jsonv::ast_node_type::object_begin:
        case jsonv::ast_node_type::array_begin:
            break;
        default:
            continue;
        }

        auto jumped = iter;
        jumped.skip_subtree();
        auto walked = walked_skip_subtree(ast, iter);
        ensure(jumped == walked);

        // Comparing node types alone would not catch the real hazard here. The tape stores a source pointer with its
        // top 8 bits truncated, and the iterator carries those bits along as it steps. A jump which recovered them
        // incorrectly still lands on the right slot and reports the right type -- it just hands back a pointer into
        // nowhere. Compare the pointers themselves.
        // Compare as `const void*`: streaming a `const char*` on failure would print it as a C string, and the whole
        // point of this check is that the pointer may not be safe to read.
        if (jumped != ast.end())
            ensure_eq(static_cast<const void*>((*walked).token_raw().data()),
                      static_cast<const void*>((*jumped).token_raw().data())
                     );

        ++structures_checked;
    }

    ensure(structures_checked > 0U);
}

TEST(ast_skip_subtree_matches_walking)
{
    ensure_skip_subtree_matches_walking("[]");
    ensure_skip_subtree_matches_walking("{}");
    ensure_skip_subtree_matches_walking("[ 1, 2, 3 ]");
    ensure_skip_subtree_matches_walking(R"({ "a": [ 1, 2, 3 ], "b": { "key": "value" }, "c": 4 })");
    ensure_skip_subtree_matches_walking(R"([ [ [ [ [ 1 ] ] ] ], [], [ {} ], { "x": [ { "y": [] } ] } ])");
    ensure_skip_subtree_matches_walking(R"({ "esc\t": "a\nb", "u": "\u00e9", "nested": { "deep": { "deeper": [] } } })");
}

TEST(ast_skip_subtree_matches_walking_on_corpus)
{
    for (const char* name : { "canada.json", "citm_catalog.json", "generated.json", "blns.json" })
    {
        std::ifstream    in(jsonv_test::test_path(name));
        std::ostringstream buffer;
        buffer << in.rdbuf();
        auto src = std::move(buffer).str();
        ensure(!src.empty());

        ensure_skip_subtree_matches_walking(src);
    }
}

/// Count the values directly inside the structure at \a pos the slow way, by tracking depth. An object's keys are not
/// counted, since each is followed by exactly one value. This is the oracle `element_count` is checked against.
static std::size_t walked_element_count(const jsonv::parse_index& index, jsonv::parse_index::iterator pos)
{
    std::size_t depth = 0U;
    std::size_t count = 0U;

    do
    {
        switch ((*pos).type())
        {
        case jsonv::ast_node_type::object_begin:
        case jsonv::ast_node_type::array_begin:
            if (depth == 1U)
                ++count;
            ++depth;
            break;
        case jsonv::ast_node_type::object_end:
        case jsonv::ast_node_type::array_end:
            --depth;
            break;
        case jsonv::ast_node_type::key_canonical:
        case jsonv::ast_node_type::key_escaped:
            break;
        default:
            if (depth == 1U)
                ++count;
            break;
        }

        ++pos;
    } while (depth > 0U && pos != index.end());

    return count;
}

/// The parser counts an array's or object's elements by its commas, which only comes to the number of elements because
/// a value which follows another without one is refused. `[1 2]` used to parse with an `element_count()` of 1.
///
/// \see https://github.com/tgockel/json-voorhees/issues/261
static void ensure_element_counts_match_walking(std::string_view src)
{
    auto ast = jsonv::parse_index::parse(src);
    ensure(ast.success());

    std::size_t structures_checked = 0U;

    for (auto iter = ast.begin(); iter != ast.end(); ++iter)
    {
        auto node = *iter;
        switch (node.type())
        {
        case jsonv::ast_node_type::object_begin:
            ensure_eq(walked_element_count(ast, iter), node.as<jsonv::ast_node::object_begin>().element_count());
            break;
        case jsonv::ast_node_type::array_begin:
            ensure_eq(walked_element_count(ast, iter), node.as<jsonv::ast_node::array_begin>().element_count());
            break;
        default:
            continue;
        }

        ++structures_checked;
    }

    ensure(structures_checked > 0U);
}

TEST(ast_element_count_matches_walking)
{
    ensure_element_counts_match_walking("[]");
    ensure_element_counts_match_walking("{}");
    ensure_element_counts_match_walking("[1, 2]");
    ensure_element_counts_match_walking("[[], []]");
    ensure_element_counts_match_walking(R"([ "a", true, false, null, -1.5, {}, [] ])");
    ensure_element_counts_match_walking(R"({ "a": [ 1, 2, 3 ], "b": { "key": "value" }, "c": 4 })");
    ensure_element_counts_match_walking(R"([ [ [ [ [ 1 ] ] ] ], [], [ {} ], { "x": [ { "y": [] } ] } ])");
    ensure_element_counts_match_walking(R"({ "esc\t": "a\nb", "u": "é", "nested": { "deep": { "deeper": [] } } })");
}

TEST(ast_element_count_matches_walking_on_corpus)
{
    for (const char* name : { "canada.json", "citm_catalog.json", "generated.json", "blns.json" })
    {
        std::ifstream    in(jsonv_test::test_path(name));
        std::ostringstream buffer;
        buffer << in.rdbuf();
        auto src = std::move(buffer).str();
        ensure(!src.empty());

        ensure_element_counts_match_walking(src);
    }
}

/// A structure which never closes has no recorded end, so there is nothing to step over.
TEST(ast_skip_subtree_of_unclosed_structure_is_end)
{
    auto ast = jsonv::parse_index::parse(R"({ "a": [ 1, 2)");
    ensure(!ast.success());

    auto iter = ast.begin();
    ++iter;     // document_start -> the `{` which never closes
    ensure(iter.skip_subtree() == ast.end());
}

TEST(ast_skip_subtree_rejects_non_structural_tokens)
{
    auto ast = jsonv::parse_index::parse(R"({ "a": 1 })");
    ensure(ast.success());

    auto iter = ast.begin();
    ++iter;     // `{`
    ++iter;     // the key
    ensure_throws(std::invalid_argument, iter.skip_subtree());
}

TEST(ast_parse_string_blns_94)
{
    auto ast = jsonv::parse_index::parse(
        //R"("\u0001\u0002\u0003\u0004\u0005\u0006\u0007\b\u000e\u000f\u0010\u0011\u0012\u0013\u0014\u0015\u0016\u0017\u0018\u0019\u001a\u001b\u001c\u001d\u001e\u001f")"
        R"("\u001f")"
    );
    ast.validate();
}
