/// \file
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/demangle.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/serialization/extractor_construction.hpp>
#include <jsonv/serialization/function_extractor.hpp>
#include <jsonv/serialization/function_serializer.hpp>
#include <jsonv/value.hpp>
#include <jsonv/detail/scope_exit.hpp>

#include <cstdint>
#include <string>
#include <tuple>
#include <typeinfo>
#include <typeindex>
#include <utility>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

struct unassociated { };

struct my_thing
{
    int a;
    int b;
    std::string c;

    my_thing(const value& from, extraction_context& cxt) :
            a(cxt.extract<int>(from.at("a"))),
            b(cxt.extract<int>(from.at("b"))),
            c(cxt.extract<std::string>(from.at("c")))
    { }

    my_thing(int a, int b, std::string c) :
            a(a),
            b(b),
            c(std::move(c))
    { }

    static const extractor* get_extractor()
    {
        static extractor_construction<my_thing> instance;
        return &instance;
    }

    static const serializer* get_serializer()
    {
        static auto instance = make_serializer<my_thing>([] (const serialization_context& context, const my_thing& self)
            {
                return object({
                               { "a", context.to_json(self.a) },
                               { "b", context.to_json(self.b) },
                               { "c", context.to_json(self.c) },
                              }
                             );
            });
        return &instance;
    }

    bool operator==(const my_thing& other) const
    {
        return std::tie(a, b, c) == std::tie(other.a, other.b, other.c);
    }

    friend std::ostream& operator<<(std::ostream& os, const my_thing& self)
    {
        return os << "{ a=" << self.a << ", b=" << self.b << ", c=" << self.c << " }";
    }

    friend std::string to_string(const my_thing& self)
    {
        std::ostringstream os;
        os << self;
        return os.str();
    }
};

}

TEST(formats_equality)
{
    formats a;
    formats b = a;
    formats c = formats::compose({ a, b });

    ensure(a == b);
    ensure(a != c);
    ensure(b == a);
    ensure(c == c);
}

// Strange cases -- defaults, global and coerce must return sub-formats so nobody can change the real ones
TEST(formats_static_results_inequality)
{
    ensure(formats::defaults() != formats::defaults());
    ensure(formats::coerce()   != formats::coerce());
    ensure(formats::global()   != formats::global());
}

TEST(formats_throws_on_duplicate)
{
    formats fmt;
    fmt.register_extractor(my_thing::get_extractor());
    ensure_throws(std::invalid_argument, fmt.register_extractor(my_thing::get_extractor()));
    fmt.register_serializer(my_thing::get_serializer());
    ensure_throws(std::invalid_argument, fmt.register_serializer(my_thing::get_serializer()));
}

TEST(extract_basics)
{
    value val = parse(R"({
                        "i": 5,
                        "d": 4.5,
                        "s": "thing",
                        "a": [ 1, 2, 3 ],
                        "o": { "i": 5, "d": 4.5 }
                      })");
    extraction_context cxt(formats::defaults());
    ensure(cxt.user_data() == nullptr);
    ensure_eq(val, cxt.extract<value>(val));
    ensure_eq(5, cxt.extract<std::int8_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint8_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int16_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint16_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int32_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint32_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int64_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint64_t>(val.at("i")));
    ensure_eq(4.5f, cxt.extract<float>(val.at("d")));
    ensure_eq(4.5, cxt.extract<double>(val.at("d")));
    ensure_eq("thing", cxt.extract<std::string>(val.at("s")));
    try
    {
        extraction_context::path_scope scope(cxt, std::string_view("o"));
        (void) cxt.extract<unassociated>(val.at("o"));
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& extract_err)
    {
        ensure_eq(path::create(".o"), extract_err.path());
        ensure(extract_err.nested_ptr());

        try
        {
            std::rethrow_exception(extract_err.nested_ptr());
        }
        catch (const no_extractor& noex)
        {
            ensure_eq(demangle(typeid(unassociated).name()), noex.type_name());
            ensure(noex.type_index() == std::type_index(typeid(unassociated)));
        }
    }
}

TEST(extract_object)
{
    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_extractor(my_thing::get_extractor());

    my_thing res = extract<my_thing>(parse(R"({ "a": 1, "b": 2, "c": "thing" })"), fmts);
    to_string(res);
    ensure_eq(my_thing(1, 2, "thing"), res);
}

TEST(extract_object_with_unique_extractor)
{
    formats fmts = formats::compose({ formats::defaults() });
    fmts.register_extractor(std::unique_ptr<extractor>(new extractor_construction<my_thing>()));

    my_thing res = extract<my_thing>(parse(R"({ "a": 1, "b": 2, "c": "thing" })"), fmts);
    ensure_eq(my_thing(1, 2, "thing"), res);
}

TEST(extract_object_search)
{
    formats base_fmts;
    base_fmts.register_extractor(my_thing::get_extractor());
    formats fmts = formats::compose({ formats::defaults(), base_fmts });

    my_thing res = extract<my_thing>(parse(R"({ "a": 1, "b": 2, "c": "thing" })"), fmts);
    ensure_eq(my_thing(1, 2, "thing"), res);
}

TEST(extract_object_with_globals)
{
    {
        formats base_fmts;
        base_fmts.register_extractor(my_thing::get_extractor());
        formats::set_global(formats::compose({ formats::defaults(), base_fmts }));
    }
    auto reset_global_on_exit = jsonv::detail::on_scope_exit([] { formats::reset_global(); });

    my_thing res = extract<my_thing>(parse(R"({ "a": 1, "b": 2, "c": "thing" })"));
    ensure_eq(my_thing(1, 2, "thing"), res);
}

TEST(extract_coerce)
{
    value val = parse(R"({
                        "i": 5,
                        "d": 4.5,
                        "s": "10",
                        "a": [ 1, 2, 3 ],
                        "o": { "i": 5, "d": 4.5 }
                      })");
    extraction_context cxt(formats::coerce());

    // regular
    ensure_eq(val, cxt.extract<value>(val));
    ensure_eq(5, cxt.extract<std::int8_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint8_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int16_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint16_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int32_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint32_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::int64_t>(val.at("i")));
    ensure_eq(5, cxt.extract<std::uint64_t>(val.at("i")));
    ensure_eq(4.5f, cxt.extract<float>(val.at("d")));
    ensure_eq(4.5, cxt.extract<double>(val.at("d")));
    ensure_eq("10", cxt.extract<std::string>(val.at("s")));

    // some coercing...
    ensure_eq("5", cxt.extract<std::string>(val.at("i")));
    ensure_eq(10, cxt.extract<int>(val.at("s")));
}

// Tests that even if we throw a completely bogus exception type, the extraction_context wraps it in an extraction_error
TEST(extractor_throws_random_thing)
{
    static auto instance = make_extractor([] (const value& from) -> unassociated { throw from; });
    formats locals;
    locals.register_extractor(&instance);

    value val = object({ { "a", 1 } });

    extraction_context cxt(locals);
    ensure_throws(extraction_error, cxt.extract<unassociated>(val));
    ensure_throws(extraction_error, cxt.extract<unassociated>(val.at("a")));
}

TEST(serialize_basics)
{
    serialization_context cxt(formats::defaults());
    ensure(cxt.user_data() == nullptr);
    ensure_eq(value(5), cxt.to_json(std::int8_t(5)));
    ensure_eq(value(5), cxt.to_json(std::uint8_t(5)));
    ensure_eq(value(5), cxt.to_json(std::int16_t(5)));
    ensure_eq(value(5), cxt.to_json(std::uint16_t(5)));
    ensure_eq(value(5), cxt.to_json(std::int32_t(5)));
    ensure_eq(value(5), cxt.to_json(std::uint32_t(5)));
    ensure_eq(value(5), cxt.to_json(std::int64_t(5)));
    ensure_eq(value(5), cxt.to_json(std::uint64_t(5)));
    ensure_eq(value(4.5), cxt.to_json(4.5));
    ensure_eq(value(4.5), cxt.to_json(4.5f));
    ensure_eq(value("thing"), cxt.to_json(std::string("thing")));

    try
    {
        (void) cxt.to_json(unassociated{});
    }
    catch (const no_serializer& noser)
    {
        ensure(noser.type_index() == std::type_index(typeid(unassociated)));
        ensure_eq(demangle(noser.type_name()), demangle(typeid(unassociated).name()));
    }
}

}
