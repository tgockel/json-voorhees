/// \file
/// The examples the documentation teaches from, compiled and run: the serialization tutorial in \c jsonv/all.hpp, the
/// opening example of the serialization builder DSL page, and the worked examples on \c jsonv::reader and
/// \c jsonv::writer. Nothing else checks code written in a Doxygen comment, and these examples have drifted from the
/// API before (#233). Each example below is the documentation's code, verbatim, except that a test asserts what an
/// example prints; a change which breaks one here has broken it there as well, so change both.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/encode.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization_builder.hpp>
#include <jsonv/serialization/container_adapter.hpp>
#include <jsonv/serialization/extractor_construction.hpp>
#include <jsonv/serialization/function_serializer.hpp>
#include <jsonv/value.hpp>
#include <jsonv/writer.hpp>

#include <cstdint>
#include <list>
#include <optional>
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// jsonv/all.hpp: Serialization                                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// The extracting constructor from "Extracting with extract".
namespace extracting
{

class my_type
{
public:
    my_type(jsonv::reader& from, jsonv::extraction_context& context)
    {
        if (!from.expect(jsonv::ast_node_type::object_begin))
            throw jsonv::extraction_error(context.problem_path(from), "Expected an object");

        // Step off the { and onto the first key -- or onto the } of an empty object.
        (void) from.next_token();
        while (from.current_type() != jsonv::ast_node_type::object_end)
        {
            std::string key = from.current().visit_key([] (const auto& k) { return std::string(k.value()); });

            // Step off the key and onto its value.
            (void) from.next_token();

            if (key == "a")
                a = extract_member<int>(from, context, "a");
            else if (key == "b")
                b = extract_member<int>(from, context, "b");
            else if (key == "c")
                c = extract_member<std::string>(from, context, "c");
            else
                (void) from.next_value();
        }

        // Step off the } too, leaving the reader one past this object.
        (void) from.next_token();
    }

    static const jsonv::extractor* get_extractor()
    {
        static jsonv::extractor_construction<my_type> instance;
        return &instance;
    }

    friend std::ostream& operator<<(std::ostream& os, const my_type& self)
    {
        return os << "{ a=" << self.a << ", b=" << self.b << ", c=" << self.c << " }";
    }

private:
    template <typename T>
    static T extract_member(jsonv::reader& from, jsonv::extraction_context& context, std::string_view key)
    {
        jsonv::extraction_context::path_scope scope(context, key);

        auto mark = context.problems().size();
        if (auto result = context.extract<T>(from))
            return *std::move(result);

        throw jsonv::extraction_error(context.take_problems_since(mark));
    }

private:
    int         a = 0;
    int         b = 0;
    std::string c;
};

}

/// The same type with the constructor the tutorial ports a 1.x one to, reading a \c value rather than the reader. Only
/// the constructor and its helper are the tutorial's.
namespace ported
{

class my_type
{
public:
    my_type(const jsonv::value& from, jsonv::extraction_context& context) :
            a(extract_member<int>(from, context, "a")),
            b(extract_member<int>(from, context, "b")),
            c(extract_member<std::string>(from, context, "c"))
    { }

    template <typename T>
    static T extract_member(const jsonv::value& from, jsonv::extraction_context& context, const std::string& key)
    {
        jsonv::extraction_context::path_scope scope(context, key);

        auto member = from.find(key);
        if (member == from.end_object())
            throw jsonv::extraction_error(context.path(), "Missing required member");

        return context.extract<T>(member->second);
    }

    static const jsonv::extractor* get_extractor()
    {
        static jsonv::extractor_construction<my_type> instance;
        return &instance;
    }

    friend std::ostream& operator<<(std::ostream& os, const my_type& self)
    {
        return os << "{ a=" << self.a << ", b=" << self.b << ", c=" << self.c << " }";
    }

private:
    int         a;
    int         b;
    std::string c;
};

}

/// The serializer from "Serialization with to_json".
namespace serializing
{

class my_type
{
public:
    my_type(int a, int b, std::string c) :
            a(a),
            b(b),
            c(std::move(c))
    { }

    static const jsonv::serializer* get_serializer()
    {
        static auto instance = jsonv::make_serializer<my_type>
                               (
                                [] (const jsonv::serialization_context& context, const my_type& self)
                                {
                                    return jsonv::object({ { "a", context.to_json(self.a) },
                                                           { "b", context.to_json(self.b) },
                                                           { "c", context.to_json(self.c) }
                                                         }
                                                        );
                                }
                               );
        return &instance;
    }

private:
    int         a;
    int         b;
    std::string c;
};

}

/// The structures from "Composing Type Adapters".
namespace composing
{

struct foo
{
    int         a;
    int         b;
    std::string c;
};

struct bar
{
    foo         x;
    foo         y;
    std::string z;
    std::string w;
};

}

/// The \c formats the tutorial's \c main builds for an extracting \c my_type, plus a \c std::vector of it, so a test
/// can check what the tutorial says happens to one which is an element of an array.
template <typename TMyType>
jsonv::formats extracting_formats()
{
    static jsonv::container_adapter<std::vector<TMyType>> vector_adapter;

    jsonv::formats local_formats;
    local_formats.register_extractor(TMyType::get_extractor());
    local_formats.register_adapter(&vector_adapter);
    return jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });
}

template <typename T>
std::string print(const T& x)
{
    std::ostringstream os;
    os << x;
    return std::move(os).str();
}

/// The \c extraction_error thrown by extracting a \c T from \a source -- JSON text or a \c value -- which the test
/// expects there to be.
template <typename T, typename TSource>
jsonv::extraction_error extraction_failure(const TSource& source, const jsonv::formats& fmts)
{
    try
    {
        (void) jsonv::extract<T>(source, fmts);
    }
    catch (const jsonv::extraction_error& ex)
    {
        return ex;
    }

    throw test_failure("extraction succeeded where it was expected to fail");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// jsonv/serialization_builder.hpp: the DSL page                                                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace dsl_page
{

struct person
{
    std::string first_name;
    std::string last_name;
    int         age;
    std::string role;
};

struct company
{
    std::string         name;
    bool                certified;
    std::vector<person> employees;
    std::list<person>   candidates;
};

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// jsonv/reader.hpp                                                                                                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace reader_class
{

struct my_object
{
    std::int64_t a = 0;
};

std::optional<my_object> extract_my_object(jsonv::reader& from)
{
    if (!from.expect(jsonv::ast_node_type::object_begin))
        return std::nullopt;

    // Step onto the first key, or onto the } of an empty object.
    if (!from.next_token())
        return std::nullopt;

    my_object out;
    while (from.good() && from.current_type() != jsonv::ast_node_type::object_end)
    {
        // Keys arrive canonical or escaped, depending on whether the source used escape sequences.
        if (!from.expect({ jsonv::ast_node_type::key_canonical, jsonv::ast_node_type::key_escaped }))
            return std::nullopt;

        auto key = from.current().visit_key([](const auto& k) { return std::string(k.value()); });
        if (key == "a")
        {
            if (!from.next_token())
                return std::nullopt;

            if (auto node = from.current_as<jsonv::ast_node::integer>())
                out.a = node->value();
            else
                return std::nullopt;

            // Step off the value and onto the next key, or onto the closing }.
            if (!from.next_token())
                return std::nullopt;
        }
        else
        {
            // A key we do not care about -- skip its value, however large, and land on the next key.
            if (!from.next_key())
                return std::nullopt;
        }
    }
    return out;
}

}

namespace reader_next_structure
{

/// Find the "a" member of an object and leave the rest of it unread.
std::optional<std::int64_t> find_a(jsonv::reader& from)
{
    if (!from.expect(jsonv::ast_node_type::object_begin))
        return std::nullopt;

    if (!from.next_token())
        return std::nullopt;

    while (from.good() && from.current_type() != jsonv::ast_node_type::object_end)
    {
        if (!from.expect({ jsonv::ast_node_type::key_canonical, jsonv::ast_node_type::key_escaped }))
            return std::nullopt;

        auto key = from.current().visit_key([](const auto& k) { return std::string(k.value()); });
        if (key != "a")
        {
            if (!from.next_key())
                return std::nullopt;

            continue;
        }

        if (!from.next_token())
            return std::nullopt;

        auto node = from.current_as<jsonv::ast_node::integer>();

        // Whatever is left of this object, we are done with it.
        (void) from.next_structure();

        if (node)
            return node->value();
        else
            return std::nullopt;
    }
    return std::nullopt;
}

}

/// A reader over \a text stepped onto its value, which is where both \c reader examples start.
jsonv::reader opened(std::string_view text)
{
    jsonv::reader out(text);
    (void) out.next_token();
    return out;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// jsonv/writer.hpp                                                                                                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace writer_class
{

struct my_object
{
    std::int64_t             a = 0;
    std::vector<std::string> tags;
};

void write_my_object(jsonv::writer& to, const my_object& from)
{
    to.object_begin();
    to.key("a").integer(from.a);
    to.key("tags").array_begin();
    for (const auto& tag : from.tags)
        to.string(tag);
    to.array_end();
    to.object_end();
}

}

}

TEST(documentation_tutorial_extract)
{
    ensure_eq(1, jsonv::extract<int>("1"));
    ensure_eq(2.5, jsonv::extract<double>("2.5"));
    ensure_eq(std::string("Hello!"), jsonv::extract<std::string>(R"("Hello!")"));

    jsonv::value val = jsonv::parse(R"({ "d": 4 })");
    ensure_eq(4, jsonv::extract<int>(val.at("d")));

    // What the prose says of the spellings: a C++ string is JSON text rather than a JSON string, and JSON which does
    // not hold what was asked for throws.
    ensure_throws(jsonv::extraction_error, jsonv::extract<std::string>("Hello!"));
    ensure_throws(jsonv::extraction_error, jsonv::extract<int>(R"("one")"));
}

TEST(documentation_tutorial_extracting_constructor)
{
    using extracting::my_type;

    // `main`, and the output shown under it.
    jsonv::formats local_formats;
    local_formats.register_extractor(my_type::get_extractor());
    jsonv::formats format = jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });

    my_type x = jsonv::extract<my_type>(R"({ "a": 1, "b": 2, "c": "Hello!" })", format);
    ensure_eq(std::string("{ a=1, b=2, c=Hello! }"), print(x));

    // Without the format, there is nothing which knows how to extract a `my_type`.
    ensure_throws(jsonv::extraction_error, jsonv::extract<my_type>(R"({ "a": 1, "b": 2, "c": "Hello!" })"));

    // Naming the source, and the message which names it.
    jsonv::extraction_context context(format,
                                      std::nullopt,
                                      jsonv::path(),
                                      nullptr,
                                      jsonv::extract_options(),
                                      "my_type.json"
                                     );
    try
    {
        my_type y = jsonv::extract<my_type>(R"({ "a": 1, "b": "two", "c": "Hello!" })", context);
        (void) y;
        ensure(!"extraction_error was not thrown");
    }
    catch (const jsonv::extraction_error& err)
    {
        ensure_eq(std::string("Extraction error at my_type.json#.b: Read node of type string when expecting integer"),
                  std::string(err.what())
                 );
    }
}

TEST(documentation_tutorial_extracting_constructor_walks_the_keys)
{
    // Keys in whatever order the document wrote them, one it does not recognize skipped however large its value is,
    // and one it leaves out leaving its member as it was initialized. An escaped key reads as the key it spells.
    using extracting::my_type;

    auto x = jsonv::extract<my_type>(R"({ "zzz": [ 1, { "q": [ 2 ] } ], "c": "x", "\u0061": 4 })",
                                     extracting_formats<my_type>()
                                    );
    ensure_eq(std::string("{ a=4, b=0, c=x }"), print(x));

    // ...and it is left one past the object, which is what lets a container read the next element.
    auto xs = jsonv::extract<std::vector<my_type>>(R"([ { "a": 1 }, {}, { "c": "y", "b": 3 } ])",
                                                   extracting_formats<my_type>()
                                                  );
    ensure_eq(3U, xs.size());
    ensure_eq(std::string("{ a=1, b=0, c= }"), print(xs[0]));
    ensure_eq(std::string("{ a=0, b=0, c= }"), print(xs[1]));
    ensure_eq(std::string("{ a=0, b=3, c=y }"), print(xs[2]));
}

TEST(documentation_tutorial_extracting_constructor_reports_once_at_the_member)
{
    // The tutorial promises that a problem with "a" is reported once, at `.a` -- or at `[3].a` when the `my_type` is
    // the fourth element of an array.
    using extracting::my_type;

    auto alone = extraction_failure<my_type>(R"({ "a": "one" })", extracting_formats<my_type>());
    ensure_eq(1U, alone.problems().size());
    ensure_eq(jsonv::path::create(".a"), alone.path());

    auto fourth = extraction_failure<std::vector<my_type>>(R"([ {}, {}, {}, { "a": "one" } ])",
                                                           extracting_formats<my_type>()
                                                          );
    ensure_eq(1U, fourth.problems().size());
    ensure_eq(jsonv::path::create("[3].a"), fourth.path());

    auto not_object = extraction_failure<std::vector<my_type>>(R"([ {}, 5 ])", extracting_formats<my_type>());
    ensure_eq(1U, not_object.problems().size());
    ensure_eq(jsonv::path::create("[1]"), not_object.path());
}

TEST(documentation_tutorial_ported_constructor)
{
    using ported::my_type;

    auto x = jsonv::extract<my_type>(R"({ "c": "Hello!", "b": 2, "a": 1 })", extracting_formats<my_type>());
    ensure_eq(std::string("{ a=1, b=2, c=Hello! }"), print(x));

    // The `path_scope` is what names the member, since the `value` it is extracted from cannot.
    auto fourth = extraction_failure<std::vector<my_type>>(R"([
                                                                 { "a": 1, "b": 2, "c": "" },
                                                                 { "a": 1, "b": 2, "c": "" },
                                                                 { "a": 1, "b": 2, "c": "" },
                                                                 { "a": 1, "b": "two", "c": "" }
                                                             ])",
                                                           extracting_formats<my_type>()
                                                          );
    ensure_eq(1U, fourth.problems().size());
    ensure_eq(jsonv::path::create("[3].b"), fourth.path());

    // A member which is not there at all is named the same way, whether the object was read from text or handed over as
    // a `value`.
    const std::string_view missing_b = R"({ "a": 1, "c": "" })";

    auto from_text = extraction_failure<my_type>(missing_b, extracting_formats<my_type>());
    ensure_eq(1U, from_text.problems().size());
    ensure_eq(jsonv::path::create(".b"), from_text.path());
    ensure_eq(std::string("Missing required member"), from_text.problems().front().message());

    auto from_value = extraction_failure<my_type>(jsonv::parse(missing_b), extracting_formats<my_type>());
    ensure_eq(1U, from_value.problems().size());
    ensure_eq(jsonv::path::create(".b"), from_value.path());

    auto fourth_missing = extraction_failure<std::vector<my_type>>(R"([
                                                                         { "a": 1, "b": 2, "c": "" },
                                                                         { "a": 1, "b": 2, "c": "" },
                                                                         { "a": 1, "b": 2, "c": "" },
                                                                         { "a": 1, "c": "" }
                                                                     ])",
                                                                   extracting_formats<my_type>()
                                                                  );
    ensure_eq(1U, fourth_missing.problems().size());
    ensure_eq(jsonv::path::create("[3].b"), fourth_missing.path());
}

TEST(documentation_tutorial_to_json)
{
    using serializing::my_type;

    jsonv::formats local_formats;
    local_formats.register_serializer(my_type::get_serializer());
    jsonv::formats format = jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });

    my_type x(5, 6, "Hello");
    ensure_eq(std::string(R"({"a":5,"b":6,"c":"Hello"})"), jsonv::to_string(jsonv::to_json(x, format)));
}

TEST(documentation_tutorial_composing_type_adapters)
{
    using composing::foo;
    using composing::bar;

    jsonv::formats formats =
        jsonv::formats_builder()
            .type<foo>()
                .member("a", &foo::a)
                .member("b", &foo::b)
                    .default_value(10)
                .member("c", &foo::c)
            .type<bar>()
                .member("x", &bar::x)
                .member("y", &bar::y)
                .member("z", &bar::z)
                    .since(jsonv::version(2, 0))
                .member("w", &bar::w)
                    .until(jsonv::version(5, 0))
            .compose_checked(jsonv::formats::defaults())
        ;

    // "perfectly capable of serializing to and extracting from this JSON document"
    const std::string_view document = R"({
                                             "x": { "a": 50, "b": 20, "c": "Blah" },
                                             "y": { "a": 10,          "c": "No B?" },
                                             "z": "Only serialized in 2.0+",
                                             "w": "Only serialized before 5.0"
                                         })";

    bar out = jsonv::extract<bar>(document, formats);
    ensure_eq(50, out.x.a);
    ensure_eq(20, out.x.b);
    ensure_eq(std::string("Blah"), out.x.c);
    ensure_eq(10, out.y.a);
    ensure_eq(10, out.y.b);
    ensure_eq(std::string("No B?"), out.y.c);
    ensure_eq(std::string("Only serialized in 2.0+"), out.z);
    ensure_eq(std::string("Only serialized before 5.0"), out.w);

    jsonv::value expected = jsonv::parse(document);
    expected["y"]["b"] = 10;
    ensure_eq(expected, jsonv::to_json(out, formats));
}

TEST(documentation_dsl_page_example)
{
    using dsl_page::person;
    using dsl_page::company;

    jsonv::formats fmts =
        jsonv::formats_builder()
            .type<person>()
                .member("first_name", &person::first_name)
                    .alternate_name("firstname")
                .member("last_name",  &person::last_name)
                    .alternate_name("lastname")
                .member("age",        &person::age)
                    .until({ 6,1 })
                    .default_value(21)
                    .default_on_null()
                    .check_input([] (int value) { if (value < 0) throw std::logic_error("Age must be positive."); })
                .member("role",       &person::role)
                    .since({ 2,0 })
                    .default_value("Builder")
            .type<company>()
                .member("name",       &company::name)
                .member("certified",  &company::certified)
                .member("employees",  &company::employees)
                .member("candidates", &company::candidates)
            .register_containers<person, std::vector, std::list>()
            .check_references(jsonv::formats::defaults())
        ;

    // The JSON the page says this describes, extracted once the DSL's adapters are combined with the defaults that
    // `check_references` checked them against.
    company out = jsonv::extract<company>(R"({
                                                 "name": "Paul's Construction",
                                                 "certified": false,
                                                 "employees": [
                                                     {
                                                         "first_name": "Bob",
                                                         "last_name":  "Builder",
                                                         "age":        29
                                                     },
                                                     {
                                                         "first_name": "James",
                                                         "last_name":  "Johnson",
                                                         "age":        38,
                                                         "role":       "Foreman"
                                                     }
                                                 ],
                                                 "candidates": [
                                                     {
                                                         "firstname": "Adam",
                                                         "lastname":  "Ant"
                                                     }
                                                 ]
                                             })",
                                          jsonv::formats::compose({ fmts, jsonv::formats::defaults() })
                                         );

    ensure_eq(std::string("Paul's Construction"), out.name);
    ensure(!out.certified);
    ensure_eq(2U, out.employees.size());
    ensure_eq(std::string("Bob"), out.employees[0].first_name);
    ensure_eq(29, out.employees[0].age);
    ensure_eq(std::string("Builder"), out.employees[0].role);
    ensure_eq(std::string("Foreman"), out.employees[1].role);
    ensure_eq(1U, out.candidates.size());
    ensure_eq(std::string("Adam"), out.candidates.front().first_name);
    ensure_eq(std::string("Ant"), out.candidates.front().last_name);
    ensure_eq(21, out.candidates.front().age);
}

TEST(documentation_reader_examples)
{
    for (std::string_view text : { R"({ "x": [ 1, { "a": 2 } ], "a": 5, "z": {} })", R"({ "\u0061": 7 })" })
    {
        // Both find "a" past the members they skip, whatever is in them, and through a key spelt with an escape.
        auto expected = jsonv::parse(text).at("a").as_integer();

        auto walked_reader = opened(text);
        auto walked        = reader_class::extract_my_object(walked_reader);
        ensure(walked.has_value());
        ensure_eq(expected, walked->a);

        auto found_reader = opened(text);
        auto found        = reader_next_structure::find_a(found_reader);
        ensure(found.has_value());
        ensure_eq(expected, *found);
    }

    // An object without an "a" is a `my_object` left as it was initialized, but there is nothing for `find_a` to find.
    auto empty_walked = opened("{}");
    auto empty        = reader_class::extract_my_object(empty_walked);
    ensure(empty.has_value());
    ensure_eq(0, empty->a);

    auto empty_found = opened("{}");
    ensure(!reader_next_structure::find_a(empty_found).has_value());

    // Anything else is nothing to either of them.
    for (std::string_view text : { R"({ "a": "five" })", R"([ 5 ])" })
    {
        auto walked_reader = opened(text);
        ensure(!reader_class::extract_my_object(walked_reader).has_value());

        auto found_reader = opened(text);
        ensure(!reader_next_structure::find_a(found_reader).has_value());
    }
}

TEST(documentation_writer_example)
{
    // The header's driver prints to `std::cout` through a pretty encoder; the same thing into a string.
    std::ostringstream            pretty;
    jsonv::ostream_pretty_encoder sink(pretty);
    jsonv::writer                 to(sink);
    writer_class::write_my_object(to, writer_class::my_object{ 1, { "x", "y" } });

    ensure_eq(std::string("{\n  \"a\": 1,\n  \"tags\": [\n    \"x\",\n    \"y\"\n  ]\n}"), pretty.str());
    ensure_eq(std::size_t(0), to.depth());

    // And compact, which is what `to_string` of the equivalent `value` prints.
    std::ostringstream compact;
    jsonv::writer      compact_to(compact);
    writer_class::write_my_object(compact_to, writer_class::my_object{ 1, { "x", "y" } });
    ensure_eq(jsonv::to_string(jsonv::parse(R"({ "a": 1, "tags": ["x", "y"] })")), compact.str());
}

}
