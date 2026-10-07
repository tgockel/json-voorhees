/// \file
/// Tests for \c jsonv::writer, the push-side mirror of \c jsonv::reader.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"
#include "allocation_counter.hpp"
#include "filesystem_util.hpp"

#include <jsonv/ast.hpp>
#include <jsonv/detail.hpp>
#include <jsonv/encode.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/value.hpp>
#include <jsonv/writer.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace jsonv_test
{

namespace
{

/// An encoder which records the hooks it is driven through and writes nothing. A token is one character; a key or a
/// string is followed by its text. The grammar tests use it to see that a refused token reached the sink with nothing.
class recording_encoder final :
        public jsonv::encoder
{
public:
    const std::string& log() const noexcept { return _log; }

protected:
    void write_null() override                           { _log += 'n'; }
    void write_object_begin() override                   { _log += '{'; }
    void write_object_end() override                     { _log += '}'; }
    void write_object_key(std::string_view key) override { _log += 'k'; _log.append(key); _log += ':'; }
    void write_object_delimiter() override               { _log += ','; }
    void write_array_begin() override                    { _log += '['; }
    void write_array_end() override                      { _log += ']'; }
    void write_array_delimiter() override                { _log += ','; }
    void write_string(std::string_view value) override   { _log += 's'; _log.append(value); }
    void write_integer(std::int64_t) override            { _log += 'i'; }
    void write_decimal(double) override                  { _log += 'd'; }
    void write_boolean(bool value) override              { _log += value ? 't' : 'f'; }

private:
    std::string _log;
};

/// An encoder which does nothing at all, for measuring what the writer itself costs.
class null_encoder final :
        public jsonv::encoder
{
protected:
    void write_null() override                     { }
    void write_object_begin() override             { }
    void write_object_end() override               { }
    void write_object_key(std::string_view) override { }
    void write_object_delimiter() override         { }
    void write_array_begin() override              { }
    void write_array_end() override                { }
    void write_array_delimiter() override          { }
    void write_string(std::string_view) override   { }
    void write_integer(std::int64_t) override      { }
    void write_decimal(double) override            { }
    void write_boolean(bool) override              { }
};

const char k_sample_json[] = R"({
  "a": [ 4, 5, 6, [7, 8, 9, {"something": 5, "else": 6}]],
  "b": "blah",
  "c": { "baz": ["bazar"], "cat": ["Eric", "Bob"] },
  "d": {},
  "e": [],
  "f": null,
  "g": [ true, false, -0.0, 1.5e300, "caf\u00e9 \ud83d\ude00" ]
})";

std::string read_file(const std::string& path)
{
    std::ifstream      in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return std::move(buffer).str();
}

/// Write what \a from reads, token for token, into \a to. This is the other way to drive a writer -- by hand, one
/// token at a time -- so a document which comes out the same both ways says the two agree about every token.
void replay(jsonv::reader& from, jsonv::writer& to)
{
    using jsonv::ast_node;
    using jsonv::ast_node_type;

    while (from.good())
    {
        const ast_node& node = from.current();
        switch (node.type())
        {
        case ast_node_type::document_start:
            break;
        case ast_node_type::document_end:
            return;
        case ast_node_type::object_begin:
            to.object_begin();
            break;
        case ast_node_type::object_end:
            to.object_end();
            break;
        case ast_node_type::array_begin:
            to.array_begin();
            break;
        case ast_node_type::array_end:
            to.array_end();
            break;
        case ast_node_type::key_canonical:
            to.key(node.as<ast_node::key_canonical>().value());
            break;
        case ast_node_type::key_escaped:
            to.key(node.as<ast_node::key_escaped>().value());
            break;
        case ast_node_type::string_canonical:
            to.string(node.as<ast_node::string_canonical>().value());
            break;
        case ast_node_type::string_escaped:
            to.string(node.as<ast_node::string_escaped>().value());
            break;
        case ast_node_type::literal_true:
            to.boolean(true);
            break;
        case ast_node_type::literal_false:
            to.boolean(false);
            break;
        case ast_node_type::literal_null:
            to.null();
            break;
        case ast_node_type::integer:
            // Through the same reading `parse` does, so a literal beyond 64 bits lands as the same `double`.
            to.write(jsonv::integer_node_value(node.as<ast_node::integer>()));
            break;
        case ast_node_type::decimal:
            to.decimal(node.as<ast_node::decimal>().value());
            break;
        case ast_node_type::error:
            ensure(!"the reader ran into a parse error");
            return;
        }

        (void) from.next_token();
    }
}

/// Check that \a parsed comes out the same whether the writer is driven by a value walk or by the tokens of a reader
/// over \a text, and over the value itself.
void ensure_replays_match(const jsonv::value& parsed, std::string_view text)
{
    const std::string expected = jsonv::to_string(parsed);

    // From the text as written, with the spacing, escapes and member order it had. A `value` sorts its members, so
    // the text can only be compared as a tree.
    std::ostringstream from_text;
    {
        jsonv::writer to(from_text);
        jsonv::reader from(text);
        replay(from, to);
        ensure_eq(std::size_t(0), to.depth());
    }
    ensure(jsonv::parse(from_text.str()) == parsed);

    // From the tree through a value-backed reader, whose member order is the one `to_string` prints.
    std::ostringstream from_value;
    {
        jsonv::writer to(from_value);
        auto          from = jsonv::reader::from_value(parsed);
        replay(from, to);
    }
    ensure(expected == from_value.str());
}

}

TEST(writer_tokens_chain_into_compact_text)
{
    std::ostringstream out;
    jsonv::writer      to(out);
    to.object_begin()
        .key("a").array_begin().integer(1).integer(2).integer(3).array_end()
        .key("b").object_begin().key("x").string("taco").object_end()
        .key("c").integer(4)
        .key("d").decimal(2.5)
        .key("e").boolean(true)
        .key("f").null()
      .object_end();

    ensure_eq(std::string(R"({"a":[1,2,3],"b":{"x":"taco"},"c":4,"d":2.5,"e":true,"f":null})"), out.str());
    ensure_eq(std::size_t(0), to.depth());
    ensure(to.good());
}

TEST(writer_write_rvalue_matches_lvalue)
{
    // A text sink has nothing to take over, so a value handed over as an rvalue is walked exactly as an lvalue is.
    const auto val = jsonv::parse(k_sample_json);

    std::ostringstream by_lvalue;
    jsonv::writer(by_lvalue).write(val);

    std::ostringstream by_rvalue;
    jsonv::writer(by_rvalue).write(jsonv::value(val));
    ensure_eq(by_lvalue.str(), by_rvalue.str());
}

TEST(writer_write_value_matches_to_string)
{
    auto val = jsonv::parse(k_sample_json);

    std::ostringstream out;
    jsonv::writer(out).write(val);
    ensure_eq(jsonv::to_string(val), out.str());

    std::ostringstream pretty_by_value;
    jsonv::ostream_pretty_encoder(pretty_by_value).encode(val);

    std::ostringstream            pretty_by_writer;
    jsonv::ostream_pretty_encoder sink(pretty_by_writer);
    jsonv::writer(sink).write(val);
    ensure_eq(pretty_by_value.str(), pretty_by_writer.str());
}

TEST(writer_ostream_constructor_writes_compact_ascii)
{
    std::ostringstream out;
    jsonv::writer(out).array_begin().string("caf\xc3\xa9").integer(1).array_end();
    ensure_eq(std::string(R"(["caf\u00e9",1])"), out.str());
}

TEST(writer_hand_driven_tokens_match_value_walk)
{
    auto val = jsonv::parse(k_sample_json);

    // Compact...
    {
        std::ostringstream out;
        jsonv::writer      to(out);
        jsonv::reader      from(jsonv::to_string(val));
        replay(from, to);
        ensure_eq(jsonv::to_string(val), out.str());
    }

    // ...and pretty, where the encoder's state machine has to see the same sequence of hook calls either way.
    {
        std::ostringstream by_value;
        jsonv::ostream_pretty_encoder(by_value).encode(val);

        std::ostringstream            by_token;
        jsonv::ostream_pretty_encoder sink(by_token);
        jsonv::writer                 to(sink);
        jsonv::reader                 from(jsonv::to_string(val));
        replay(from, to);
        ensure_eq(by_value.str(), by_token.str());
    }
}

TEST(writer_replay_matches_to_string_on_corpus)
{
    for (const char* name : { "blns.json", "paths.json", "canada.json", "citm_catalog.json", "generated.json" })
    {
        auto src = read_file(test_path(name));
        ensure(!src.empty());
        ensure_replays_match(jsonv::parse(src), src);
    }
}

TEST(writer_replay_matches_to_string_on_fuzz_corpus)
{
    // `test_path` is rooted at `src/jsonv-tests/data`, and the fuzz seeds are a sibling of that tree.
    auto root = test_path("../../jsonv-fuzz/corpus");

    std::size_t compared = 0U;
    recursive_directory_for_each(root, ".json", [&](const std::string& path)
        {
            auto src = read_file(path);
            ensure(!src.empty());

            jsonv::value parsed;
            try
            {
                parsed = jsonv::parse(src);
            }
            catch (const std::exception&)
            {
                // A seed which does not parse, or holds a number no `double` can, has no value to compare.
                return;
            }

            // A seed holding bytes which are not valid UTF-8 does not survive a trip through the encoder and back, as
            // the reader tests note. That is a defect on the text side which the writer only inherits, so such a seed
            // cannot serve as a baseline.
            try
            {
                if (jsonv::parse(jsonv::to_string(parsed)) != parsed)
                    return;
            }
            catch (const std::exception&)
            {
                return;
            }

            ensure_replays_match(parsed, src);
            ++compared;
        });

    ensure_ge(compared, std::size_t(8U));
}

TEST(writer_key_outside_object_throws)
{
    recording_encoder sink;
    jsonv::writer     to(sink);

    ensure_throws(std::logic_error, to.key("a"));
    ensure_eq(std::string(), sink.log());

    to.array_begin();
    ensure_throws(std::logic_error, to.key("a"));
    ensure_eq(std::string("["), sink.log());
}

TEST(writer_value_without_key_throws)
{
    recording_encoder sink;
    jsonv::writer     to(sink);

    to.object_begin();
    ensure_throws(std::logic_error, to.null());
    ensure_throws(std::logic_error, to.boolean(true));
    ensure_throws(std::logic_error, to.integer(1));
    ensure_throws(std::logic_error, to.decimal(1.5));
    ensure_throws(std::logic_error, to.string("s"));
    ensure_throws(std::logic_error, to.object_begin());
    ensure_throws(std::logic_error, to.array_begin());
    ensure_throws(std::logic_error, to.write(jsonv::value(1)));
    ensure_eq(std::string("{"), sink.log());

    // A refused token leaves the writer where it was, so the right token still works...
    to.key("a").integer(1);
    ensure_eq(std::string("{ka:i"), sink.log());

    // ...and a second value for the same key is refused like the first.
    ensure_throws(std::logic_error, to.string("again"));
    ensure_eq(std::string("{ka:i"), sink.log());

    to.object_end();
    ensure_eq(std::string("{ka:i}"), sink.log());
}

TEST(writer_key_after_key_throws)
{
    recording_encoder sink;
    jsonv::writer     to(sink);

    to.object_begin().key("a");
    ensure_throws(std::logic_error, to.key("b"));
    ensure_eq(std::string("{ka:"), sink.log());
}

TEST(writer_mismatched_end_throws)
{
    recording_encoder sink;
    jsonv::writer     to(sink);

    ensure_throws(std::logic_error, to.object_end());
    ensure_throws(std::logic_error, to.array_end());
    ensure_eq(std::string(), sink.log());

    to.array_begin();
    ensure_throws(std::logic_error, to.object_end());

    to.object_begin();
    ensure_throws(std::logic_error, to.array_end());

    to.key("k");
    ensure_throws(std::logic_error, to.object_end());
    ensure_eq(std::string("[{kk:"), sink.log());

    to.null().object_end().array_end();
    ensure_eq(std::string("[{kk:n}]"), sink.log());
    ensure_eq(std::size_t(0), to.depth());
}

TEST(writer_delimiters_are_the_writers_job)
{
    recording_encoder sink;
    jsonv::writer     to(sink);

    to.array_begin()
        .integer(1)
        .object_begin().key("a").integer(1).key("b").array_begin().array_end().object_end()
        .array_begin().null().null().array_end()
      .array_end();

    ensure_eq(std::string("[i,{ka:i,kb:[]},[n,n]]"), sink.log());
}

TEST(writer_depth_tracks_open_structures)
{
    recording_encoder sink;
    jsonv::writer     to(sink);

    ensure_eq(std::size_t(0), to.depth());
    to.array_begin();
    ensure_eq(std::size_t(1), to.depth());
    to.object_begin();
    ensure_eq(std::size_t(2), to.depth());
    to.key("a").array_begin();
    ensure_eq(std::size_t(3), to.depth());
    to.integer(1);
    ensure_eq(std::size_t(3), to.depth());
    to.array_end();
    ensure_eq(std::size_t(2), to.depth());
    to.object_end();
    ensure_eq(std::size_t(1), to.depth());
    to.array_end();
    ensure_eq(std::size_t(0), to.depth());
}

TEST(writer_current_path_names_the_next_slot)
{
    recording_encoder sink;
    jsonv::writer     to(sink);
    auto at = [&] { return jsonv::to_string(to.current_path()); };

    ensure_eq(std::string("."), at());
    to.object_begin();
    ensure_eq(std::string("."), at());
    to.key("a");
    ensure_eq(std::string(".a"), at());
    to.array_begin();
    ensure_eq(std::string(".a[0]"), at());
    to.integer(1);
    ensure_eq(std::string(".a[1]"), at());
    to.integer(2);
    ensure_eq(std::string(".a[2]"), at());
    to.array_end();
    ensure_eq(std::string("."), at());
    to.key("b");
    ensure_eq(std::string(".b"), at());
    to.object_begin();
    ensure_eq(std::string(".b"), at());
    to.key("x");
    ensure_eq(std::string(".b.x"), at());
    to.string("taco");
    ensure_eq(std::string(".b"), at());
    to.object_end();
    ensure_eq(std::string("."), at());
    to.key("c");
    ensure_eq(std::string(".c"), at());
    to.integer(4);
    ensure_eq(std::string("."), at());
    to.object_end();
    ensure_eq(std::string("."), at());

    // A second root starts over.
    to.array_begin();
    ensure_eq(std::string("[0]"), at());
    to.array_begin();
    ensure_eq(std::string("[0][0]"), at());
    to.array_end();
    ensure_eq(std::string("[1]"), at());

    // A key which is not an identifier prints the way `path` prints it.
    to.object_begin().key("two words");
    ensure_eq(std::string(R"([1]["two words"])"), at());
}

TEST(writer_current_path_survives_a_refused_token)
{
    recording_encoder sink;
    jsonv::writer     to(sink);
    to.object_begin().key("a").array_begin().integer(1);

    ensure_throws(std::logic_error, to.key("nope"));
    ensure_throws(std::logic_error, to.object_end());
    ensure_eq(std::string(".a[1]"), jsonv::to_string(to.current_path()));
}

TEST(writer_two_roots_in_one_stream)
{
    std::ostringstream out;
    jsonv::writer      to(out);
    to.object_begin().object_end().array_begin().integer(1).array_end().integer(2);
    ensure_eq(std::string("{}[1]2"), out.str());

    // Which is what two calls to `encode` on one encoder have always produced.
    std::ostringstream          twice;
    jsonv::ostream_encoder      encoder(twice);
    encoder.encode(jsonv::object());
    encoder.encode(jsonv::array({ 1 }));
    encoder.encode(jsonv::value(2));
    ensure_eq(twice.str(), out.str());
}

TEST(writer_moved_from_is_not_good)
{
    std::ostringstream out;
    jsonv::writer      source(out);
    source.array_begin();

    jsonv::writer target(std::move(source));
    ensure(!source.good());
    ensure_eq(std::size_t(0), source.depth());
    ensure_throws(std::invalid_argument, source.current_path());
    ensure_throws(std::invalid_argument, source.integer(1));
    ensure_throws(std::invalid_argument, source.key("a"));
    ensure_throws(std::invalid_argument, source.array_end());
    ensure_throws(std::invalid_argument, source.write(jsonv::value(1)));

    ensure(target.good());
    ensure_eq(std::size_t(1), target.depth());
    target.integer(1);

    std::ostringstream other;
    jsonv::writer      assigned(other);
    assigned = std::move(target);
    ensure(!target.good());
    ensure(assigned.good());
    assigned.array_end();

    ensure_eq(std::string("[1]"), out.str());
    ensure_eq(std::string(), other.str());
}

TEST(writer_deep_nesting)
{
    constexpr std::size_t depth = 4096U;

    std::ostringstream out;
    jsonv::writer      to(out);
    for (std::size_t idx = 0U; idx < depth; ++idx)
        to.array_begin();
    ensure_eq(depth, to.depth());
    for (std::size_t idx = 0U; idx < depth; ++idx)
        to.array_end();
    ensure_eq(std::size_t(0), to.depth());

    ensure_eq(std::string(depth, '[') + std::string(depth, ']'), out.str());
}

TEST(writer_deep_nesting_write)
{
    // The walk an encoder producing text makes of a whole tree recurses; this is the deepest it is asked to go.
    constexpr std::size_t depth = 4096U;

    jsonv::value nested = jsonv::array();
    for (std::size_t level = 1U; level < depth; ++level)
    {
        jsonv::value outer = jsonv::array();
        outer.push_back(std::move(nested));
        nested = std::move(outer);
    }

    std::ostringstream out;
    jsonv::writer(out).write(nested);
    ensure_eq(std::string(depth, '[') + std::string(depth, ']'), out.str());
}

#if JSONV_TEST_COUNTS_ALLOCATIONS

TEST(writer_steady_state_allocates_nothing)
{
    null_encoder  sink;
    jsonv::writer to(sink);

    auto write_document = [&]
        {
            to.object_begin()
                .key("name").string("value")
                .key("list").array_begin().integer(1).decimal(2.5).boolean(true).null().array_end()
                .key("nested").object_begin().key("a-key-longer-than-the-small-string-buffer").string("x").object_end()
              .object_end();
        };

    // The first document grows the frame stack and the key buffers.
    write_document();

    allocation_counter allocations;
    write_document();
    const std::size_t cost = allocations.count();
    ensure_eq(std::size_t(0), cost);
}

#endif

}
