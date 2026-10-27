/// \file
/// Tests for \c jsonv::documents, which reads several JSON documents out of one input.
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
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/value.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace jsonv_test
{

namespace
{

/// Every document in \a docs, each deserialized as a \c value, as the elements of an array.
jsonv::value values_of(jsonv::documents docs)
{
    jsonv::value out = jsonv::array();
    for (jsonv::reader& doc : docs)
        out.push_back(jsonv::deserialize<jsonv::value>(doc));
    return out;
}

/// The \c parse_error \a doc's \c reader::validate throws; or nothing, if its document parsed.
std::optional<jsonv::parse_error> parse_failure_of(const jsonv::reader& doc)
{
    try
    {
        doc.validate();
        return std::nullopt;
    }
    catch (const jsonv::parse_error& ex)
    {
        return ex;
    }
}

static_assert(std::input_iterator<jsonv::documents::iterator>);
static_assert(std::sentinel_for<std::default_sentinel_t, jsonv::documents::iterator>);
static_assert(std::ranges::input_range<jsonv::documents>);

}

TEST(documents_json_lines)
{
    auto expected = jsonv::parse(R"([ { "a": 1 }, { "a": 2 }, { "a": 3 } ])");

    ensure_eq(expected, values_of(jsonv::documents("{\"a\":1}\n{\"a\":2}\n{\"a\":3}\n")));
    ensure_eq(expected, values_of(jsonv::documents("{\"a\":1}\n{\"a\":2}\n{\"a\":3}")));
    ensure_eq(expected, values_of(jsonv::documents("{\"a\":1}\r\n{\"a\":2}\r\n{\"a\":3}\r\n")));
}

TEST(documents_concatenated)
{
    ensure_eq(jsonv::parse(R"([ { "a": 1 }, { "a": 2 } ])"), values_of(jsonv::documents(R"({"a":1}{"a":2})")));
    ensure_eq(jsonv::parse(R"([ [ 1 ], [ 2 ] ])"),           values_of(jsonv::documents("[1][2]")));
    ensure_eq(jsonv::parse(R"([ {}, {} ])"),                 values_of(jsonv::documents("{}{}")));
    ensure_eq(jsonv::parse(R"([ "a", "b" ])"),               values_of(jsonv::documents(R"("a""b")")));
    ensure_eq(jsonv::parse(R"([ 1, [ 2 ] ])"),               values_of(jsonv::documents("1[2]")));
}

TEST(documents_pretty_printed)
{
    std::string_view source = R"({
    "a": [
        1,
        2
    ]
}
{
    "b": {
        "c": null
    }
}
[
    true
]
)";

    ensure_eq(jsonv::parse(R"([ { "a": [ 1, 2 ] }, { "b": { "c": null } }, [ true ] ])"),
              values_of(jsonv::documents(source))
             );
}

TEST(documents_scalars)
{
    ensure_eq(jsonv::parse(R"([ 1, 2, "x", true, false, null, -3.5 ])"),
              values_of(jsonv::documents(R"(1 2 "x" true false null -3.5)"))
             );

    // A number goes on for as long as its digits do, so two of them need something between them.
    ensure_eq(jsonv::parse("[ 12 ]"), values_of(jsonv::documents("12")));
}

TEST(documents_deserialize_each_as_a_type)
{
    std::vector<std::int64_t> out;
    for (jsonv::reader& doc : jsonv::documents("1\n2\n3\n"))
        out.push_back(jsonv::deserialize<std::int64_t>(doc));

    ensure(out == (std::vector<std::int64_t>{ 1, 2, 3 }));
}

TEST(documents_none_in_input_without_a_value)
{
    for (std::string_view source : { "", " ", "\n\n", " \t\r\n " })
    {
        jsonv::documents docs(source);
        ensure(docs.begin() == docs.end());
    }

    // A default-constructed view has no storage behind it, so where the parse stopped is a null pointer.
    jsonv::documents null_backed{ std::string_view() };
    ensure(null_backed.begin() == null_backed.end());

    jsonv::documents empty_owned{ std::string() };
    ensure(empty_owned.begin() == empty_owned.end());

    jsonv::documents comments_only("/* a */ \n /* b */", jsonv::parse_options().comments(true));
    ensure(comments_only.begin() == comments_only.end());
}

TEST(documents_whitespace_after_the_last_is_not_a_document)
{
    ensure_eq(jsonv::parse("[ [ 1 ] ]"), values_of(jsonv::documents("  [1]  \n\n  ")));

    // Not even under `require_document`, which would otherwise refuse it as a document which is not an object or array.
    auto strict = jsonv::parse_options().require_document(true);
    ensure_eq(jsonv::parse("[ [ 1 ], { } ]"), values_of(jsonv::documents("[1]\n{}\n", strict)));
}

TEST(documents_comments_between_documents)
{
    std::string_view source = "[1] /* one */\n[2] /* two */";

    ensure_eq(jsonv::parse("[ [ 1 ], [ 2 ] ]"),
              values_of(jsonv::documents(source, jsonv::parse_options().comments(true)))
             );

    // RFC 8259 has no comments, so by default the first one is where the second document fails.
    jsonv::documents docs(source);
    auto             iter = docs.begin();
    ensure(iter != docs.end());
    ensure(!parse_failure_of(*iter));

    ++iter;
    ensure(iter != docs.end());
    auto failure = parse_failure_of(*iter);
    ensure(failure);
    ensure_eq(source.find('/'), failure->character().value());

    ++iter;
    ensure(iter == docs.end());
}

TEST(documents_failure_is_the_last_document)
{
    std::string_view source = "{\"a\":1}\n{\"a\":2}\n{\"a\":}\n{\"a\":4}\n";

    jsonv::documents docs(source);
    auto             iter = docs.begin();

    ensure_eq(jsonv::parse(R"({ "a": 1 })"), jsonv::deserialize<jsonv::value>(*iter));
    ++iter;
    ensure_eq(jsonv::parse(R"({ "a": 2 })"), jsonv::deserialize<jsonv::value>(*iter));
    ++iter;

    // The third does not parse. It is still handed out, so what is wrong with it can be asked, and the position is in
    // the whole input rather than in the document.
    ensure(iter != docs.end());
    auto failure = parse_failure_of(*iter);
    ensure(failure);
    ensure_eq(source.find("}\n{\"a\":4"), failure->character().value());
    ensure(std::string_view(failure->what()).ends_with(to_string(jsonv::ast_error::close_after_comma)));
    ensure_throws(jsonv::deserialization_error, jsonv::deserialize<jsonv::value>(*iter));

    // Where it would have ended is not known, so the fourth is never reached.
    ++iter;
    ensure(iter == docs.end());
}

TEST(documents_parse_options_apply_to_each)
{
    auto options = jsonv::parse_options().require_document(true).max_structure_depth(2);

    std::string_view source = "[1] {} 3 [[4]]";
    {
        jsonv::documents docs(source, options);
        auto             iter = docs.begin();
        ensure(!parse_failure_of(*iter));
        ++iter;
        ensure(!parse_failure_of(*iter));
        ++iter;

        // A document which is not an object or array is reported where it starts, which is where the one before it
        // ended.
        auto failure = parse_failure_of(*iter);
        ensure(failure);
        ensure_eq(source.find(" 3"), failure->character().value());
        ensure(std::string_view(failure->what()).ends_with(to_string(jsonv::ast_error::expected_document)));
        ++iter;
        ensure(iter == docs.end());
    }

    {
        jsonv::documents docs("[1] [2] [[3]]", options);
        auto             iter = docs.begin();
        ++iter;
        ++iter;
        auto failure = parse_failure_of(*iter);
        ensure(failure);
        ensure(std::string_view(failure->what()).ends_with(to_string(jsonv::ast_error::depth_exceeded)));
    }

    auto             beyond_limit = jsonv::parse_options::k::max_structure_depth + 1;
    jsonv::documents too_deep("[1]", jsonv::parse_options().max_structure_depth(beyond_limit));
    ensure_throws(std::invalid_argument, too_deep.begin());
}

TEST(documents_reader_reads_only_its_document)
{
    jsonv::documents docs("[1] [2]");
    auto             iter = docs.begin();
    jsonv::reader&   doc  = *iter;

    ensure(doc.current_type() == jsonv::ast_node_type::document_start);
    ensure(doc.next_token());
    ensure(doc.current_type() == jsonv::ast_node_type::array_begin);
    ensure(doc.next_value());
    ensure(doc.current_type() == jsonv::ast_node_type::document_end);
    ensure(!doc.next_token());
    ensure(!doc.good());
}

TEST(documents_begin_again_continues_where_it_was)
{
    jsonv::documents docs("1 2 3");
    auto             iter = docs.begin();
    ++iter;

    auto again = docs.begin();
    ensure_eq(2, jsonv::deserialize<std::int64_t>(*again));
}

TEST(documents_at_the_end)
{
    jsonv::documents docs("1");
    auto             iter = docs.begin();
    ++iter;
    ensure(iter == docs.end());

    ++iter;
    ensure(iter == docs.end());
    ensure_throws(std::logic_error, *iter);

    jsonv::documents::iterator unattached;
    ensure(unattached == docs.end());
    ensure_throws(std::logic_error, *unattached);
}

TEST(documents_owned_source_survives_a_move)
{
    // Short enough to be stored inside the `std::string`, where moving the string would move the text.
    jsonv::documents docs(std::string("1 2"));
    jsonv::documents moved(std::move(docs));

    ensure_eq(jsonv::parse("[ 1, 2 ]"), values_of(std::move(moved)));
    ensure_throws(std::invalid_argument, docs.begin());
}

TEST(documents_iterator_survives_a_move_of_its_documents)
{
    jsonv::documents docs(std::string("1 2"));
    auto             iter = docs.begin();

    jsonv::documents moved(std::move(docs));
    ensure_eq(1, jsonv::deserialize<std::int64_t>(*iter));
    ++iter;
    ensure_eq(2, jsonv::deserialize<std::int64_t>(*iter));
    ++iter;
    ensure(iter == moved.end());
}

TEST(documents_from_istream)
{
    std::istringstream in("xx[1]\n[2] x");
    (void) in.get();
    (void) in.get();

    jsonv::documents docs(in);
    auto             iter = docs.begin();
    ensure_eq(jsonv::parse("[ 1 ]"), jsonv::deserialize<jsonv::value>(*iter));
    ++iter;
    ensure_eq(jsonv::parse("[ 2 ]"), jsonv::deserialize<jsonv::value>(*iter));
    ++iter;

    // Counted from where the stream was, not from its beginning.
    auto failure = parse_failure_of(*iter);
    ensure(failure);
    ensure_eq(8U, failure->character().value());
}

TEST(documents_views_of_a_caller_source_outlive_the_iterator)
{
    std::string                   text = R"("first" "second")";
    std::vector<std::string_view> views;
    for (jsonv::reader& doc : jsonv::documents(std::string_view(text)))
        views.push_back(jsonv::deserialize<std::string_view>(doc));

    ensure_eq(2U, views.size());
    ensure_eq(std::string_view("first"),  views[0]);
    ensure_eq(std::string_view("second"), views[1]);
    ensure(views[0].data() == text.data() + 1);
}

}
