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

    my_thing(reader& from, extraction_context& cxt)
    {
        if (!cxt.current_as<ast_node::object_begin>(from))
            throw extraction_error(from.current_path(), "Expected an object");

        while (from.next_token())
        {
            auto current = from.current();
            if (current.type() == ast_node_type::key_canonical)
            {
                auto key = current.as<ast_node::key_canonical>().value();
                if (key == "a")
                {
                    if (!from.next_token())
                        throw extraction_error(from.current_path(), "Unexpected EOF");

                    if (auto x = cxt.extract<int>(from))
                    {
                        a = x.value();
                    }
                    else
                    {
                        throw extraction_error(from.current_path(), "Failed to extract \"a\"");
                    }
                }
                else if (key == "b")
                {
                    if (!from.next_token())
                        throw extraction_error(from.current_path(), "Unexpected EOF");

                    if (auto x = cxt.extract<int>(from))
                    {
                        b = x.value();
                    }
                    else
                    {
                        throw extraction_error(from.current_path(), "Failed to extract \"b\"");
                    }
                }
                else if (key == "c")
                {
                    if (!from.next_token())
                        throw extraction_error(from.current_path(), "Unexpected EOF");

                    if (auto x = cxt.extract<std::string>(from))
                    {
                        c = x.value();
                    }
                    else
                    {
                        throw extraction_error(from.current_path(), "Failed to extract \"c\"");
                    }
                }
            }
            else if (current.type() == ast_node_type::object_end)
            {
                break;
            }
            else
            {
            }
        }
    }

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

TEST(extract_basic_int32)
{
    reader rdr("5");
    ensure_eq(5, extract<std::int32_t>(rdr));
}

TEST(extract_basic_uint32)
{
    reader rdr("5");
    ensure_eq(5U, extract<std::uint32_t>(rdr));
}

TEST(extract_basic_int8)
{
    reader rdr("9");
    ensure_eq(std::int8_t(9), extract<std::int8_t>(rdr));
}

TEST(extract_basic_float)
{
    reader rdr("4.5");
    ensure_eq(4.5f, extract<float>(rdr));
}

TEST(extract_basic_double)
{
    reader rdr("-8.5");
    ensure_eq(-8.5, extract<double>(rdr));
}

TEST(extract_basic_string_canonical)
{
    reader rdr("\"canonical form\"");
    ensure_eq("canonical form", extract<std::string>(rdr));
}

TEST(extract_basic_string_escaped)
{
    reader rdr(R"("\u0068\u0065\u006c\u006c\u006f")");
    ensure_eq("hello", extract<std::string>(rdr));
}

TEST(extract_no_extractor)
{
    reader rdr("{}");
    try
    {
        extract<unassociated>(rdr);
    }
    catch (const extraction_error& extract_err)
    {
        ensure_eq(path::create("."), extract_err.path());
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

    my_thing res = extract<my_thing>(reader(R"({ "a": 1, "b": 2, "c": "thing" })"), fmts);
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

    my_thing res = extract<my_thing>(reader(R"({ "a": 1, "b": 2, "c": "thing" })"));
    ensure_eq(my_thing(1, 2, "thing"), res);
}

TEST(extract_coerce_int32)
{
    ensure_eq(5, extract<std::int32_t>(reader("5"), formats::coerce()));
}

TEST(extract_coerce_int32_to_string)
{
    ensure_eq("5", extract<std::string>(reader("5"), formats::coerce()));
}

TEST(extract_coerce_string_to_int32)
{
    ensure_eq(10, extract<std::int32_t>(reader("\"10\""), formats::coerce()));
}

// Tests that even if we throw a completely bogus exception type, the extraction_context wraps it in an extraction_error
TEST(extractor_throws_random_thing)
{
    static auto instance = make_extractor([] (reader&) -> unassociated { throw "Who knows?"; });
    formats locals;
    locals.register_extractor(&instance);

    value val = object({ { "a", 1 } });

    ensure_throws(extraction_error, extract<unassociated>(val, locals));
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
        cxt.to_json(unassociated{});
    }
    catch (const no_serializer& noser)
    {
        ensure(noser.type_index() == std::type_index(typeid(unassociated)));
        ensure_eq(demangle(noser.type_name()), demangle(typeid(unassociated).name()));
    }
}

}
