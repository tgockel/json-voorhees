/** \file
 *
 *  Copyright (c) 2015-2019 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/


#include "test.hpp"

#include <jsonv/parse.hpp>
#include <jsonv/serialization_builder.hpp>
#include <jsonv/serialization_optional.hpp>

#include <set>
#include <sstream>
#include <tuple>
#include <vector>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

struct person
{
    person() = default;

    person(std::string       f,
           std::string       l,
           int               a,
           std::set<long>    favorite_numbers = std::set<long>{},
           std::vector<long> winning_numbers  = std::vector<long>{},
           optional<std::string>  m = nullopt
          ) :
            firstname(std::move(f)),
            middle_name(m),
            lastname(std::move(l)),
            age(a),
            favorite_numbers(std::move(favorite_numbers)),
            winning_numbers(std::move(winning_numbers))
    { }

    std::string           firstname;
    optional<std::string> middle_name;
    std::string           lastname;
    int                   age;
    std::set<long>        favorite_numbers;
    std::vector<long>     winning_numbers;

    bool operator==(const person& other) const
    {
        return std::tie(firstname,       middle_name,       lastname,       age,       favorite_numbers,       winning_numbers)
            == std::tie(other.firstname, other.middle_name, other.lastname, other.age, other.favorite_numbers, other.winning_numbers);
    }

    friend std::ostream& operator<<(std::ostream& os, const person& p)
    {
        return os << p.firstname << " " << (p.middle_name ? *p.middle_name+" " : "") << p.lastname << " (" << p.age << ")";
    }

    friend std::string to_string(const person& p)
    {
        std::ostringstream os;
        os << p;
        return os.str();
    }
};

struct sometype
{
    int64_t v;
};

}

TEST(serialization_builder_members)
{
    formats fmt = formats_builder()
                    .type<person>()
                        .member("firstname", &person::firstname)
                        .member("middle_name", &person::middle_name)
                        .member("lastname",  &person::lastname)
                        .member("age",       &person::age)
                        .register_optional<optional<std::string>>()
                    .compose_checked(formats::defaults())
                ;

    person p("Bob", "Builder", 29);
    to_string(p);
    value expected = object({ { "firstname",    p.firstname },
                              { "middle_name",  jsonv::null },
                              { "lastname",     p.lastname  },
                              { "age",          p.age       }
                            }
                           );
    value encoded = to_json(p, fmt);
    ensure_eq(expected, encoded);

    person q = extract<person>(encoded, fmt);
    ensure_eq(expected, encoded);
}

TEST(serialization_builder_members_since)
{
    using my_pair = std::pair<int, int>;

    formats fmt =
        formats_builder()
            .type<my_pair>()
                .member("a", &my_pair::first)
                .member("b", &my_pair::second)
                    .since({ 2, 0 })
            .compose_checked(formats::defaults())
        ;

    auto to_json_ver = [&fmt] (const version& v)
                       {
                        serialization_context context(fmt, v);
                        return context.to_json(my_pair(5, 10));
                       };

    ensure_eq(0U, to_json_ver({ 1, 0 }).count("b"));
    ensure_eq(1U, to_json_ver({ 2, 0 }).count("b"));
    ensure_eq(1U, to_json_ver({ 3, 0 }).count("b"));
}

TEST(serialization_builder_container_members)
{
    formats fmt = formats_builder()
                    .type<person>()
                        .member("firstname",        &person::firstname)
                        .member("lastname",         &person::lastname)
                        .member("age",              &person::age)
                        .member("favorite_numbers", &person::favorite_numbers)
                        .member("winning_numbers",  &person::winning_numbers)
                    #if JSONV_COMPILER_SUPPORTS_TEMPLATE_TEMPLATES
                    .register_containers<long, std::set, std::vector>()
                    #else
                    .register_container<std::set<long>>()
                    .register_container<std::vector<long>>()
                    #endif
                    .compose_checked(formats::defaults())
                ;

    person p("Bob", "Builder", 29, { 1, 2, 3, 4 }, { 5, 6, 7, 8 });
    value expected = object({ { "firstname",        p.firstname          },
                              { "lastname",         p.lastname           },
                              { "age",              p.age                },
                              { "favorite_numbers", array({ 1, 2, 3, 4 })},
                              { "winning_numbers",  array({ 5, 6, 7, 8 })},
                            }
                           );
    auto encoded = to_json(p, fmt);
    ensure_eq(expected, encoded);
    person q = extract<person>(encoded, fmt);
    ensure_eq(p, q);
}

TEST(serialization_builder_extract_extra_keys)
{
    std::set<std::string> extra_keys;
    auto extra_keys_handler = [&extra_keys] (const extraction_context&, const value&, std::set<std::string> x)
                              {
                                  extra_keys = std::move(x);
                              };

    formats fmt = formats_builder()
                    .type<person>()
                        .member("firstname", &person::firstname)
                        .member("lastname",  &person::lastname)
                        .member("age",       &person::age)
                        .on_extract_extra_keys(extra_keys_handler)
                    .compose_checked(formats::defaults())
                ;

    person p("Bob", "Builder", 29);
    value encoded = object({ { "firstname", p.firstname },
                             { "lastname",  p.lastname  },
                             { "age",       p.age       },
                             { "extra1",    10          },
                             { "extra2",    "pie"       }
                           }
                          );

    person q = extract<person>(encoded, fmt);
    ensure_eq(p, q);
    ensure(extra_keys == std::set<std::string>({ "extra1", "extra2" }));
}

TEST(serialization_builder_post_extract_single)
{
    auto post_extract_handler = [] (const extraction_context&, person&& p) -> person
                              {
                                  p.age++;
                                  return p;
                              };

    formats fmt = formats_builder()
                    .type<person>()
                        .post_extract(post_extract_handler)
                        .member("firstname", &person::firstname)
                        .member("lastname",  &person::lastname)
                        .member("age",       &person::age)
                    .compose_checked(formats::defaults())
                ;

    person p("Bob", "Builder", 29);
    auto encoded = to_json(p, fmt);
    person q = extract<person>(encoded, fmt);
    ensure_eq(person("Bob", "Builder", 30), q);
}

TEST(serialization_builder_post_extract_multi)
{
    auto post_extract_handler_1 = [] (const extraction_context&, person&& p)
                                {
                                    p.age++;
                                    return p;
                                };

    auto post_extract_handler_2 = [] (const extraction_context&, person&& p)
                                {
                                    p.lastname = "Mc" + p.lastname;
                                    return p;
                                };

    formats fmt = formats_builder()
                    .type<person>()
                        .post_extract(post_extract_handler_1)
                        .post_extract(post_extract_handler_2)
                        .member("firstname", &person::firstname)
                        .member("lastname",  &person::lastname)
                        .member("age",       &person::age)
                    .compose_checked(formats::defaults())
                ;

    person p("Bob", "Builder", 29);
    auto encoded = to_json(p, fmt);
    person q = extract<person>(encoded, fmt);
    ensure_eq(person("Bob", "McBuilder", 30), q);
}

TEST(serialization_builder_defaults)
{
    formats fmt = formats_builder()
                    .type<person>()
                        .member("firstname",        &person::firstname)
                        .member("lastname",         &person::lastname)
                        .member("age",              &person::age)
                            .default_value(20)
                        .member("favorite_numbers", &person::favorite_numbers)
                        .member("winning_numbers",  &person::winning_numbers)
                            .default_value([] (const extraction_context& cxt, const value& val)
                                           {
                                               return cxt.extract_sub<std::vector<long>>(val, "favorite_numbers");
                                           }
                                          )
                            .default_on_null()
                    #if JSONV_COMPILER_SUPPORTS_TEMPLATE_TEMPLATES
                    .register_containers<long, std::set, std::vector>()
                    #else
                    .register_container<std::set<long>>()
                    .register_container<std::vector<long>>()
                    #endif
                    .compose_checked(formats::defaults())
                ;

    person p("Bob", "Builder", 20, { 1, 2, 3, 4 }, { 1, 2, 3, 4 });
    value input = object({ { "firstname",        p.firstname          },
                           { "lastname",         p.lastname           },
                           { "favorite_numbers", array({ 1, 2, 3, 4 })},
                           { "winning_numbers",  null                 },
                         }
                        );
    auto encoded = to_json(p, fmt);
    person q = extract<person>(encoded, fmt);
    ensure_eq(p, q);
}

TEST(serialization_builder_encode_checks)
{
    formats fmt = formats_builder()
                    .type<person>()
                        .member("firstname",        &person::firstname)
                        .member("lastname",         &person::lastname)
                        .member("age",              &person::age)
                            .encode_if([] (const serialization_context&, int age) { return age > 20; })
                        .member("favorite_numbers", &person::favorite_numbers)
                            .encode_if([] (const serialization_context&, const std::set<long>& nums) { return nums.size(); })
                        .member("winning_numbers",  &person::winning_numbers)
                            .encode_if([] (const serialization_context&, const std::vector<long>& nums) { return nums.size(); })
                    #if JSONV_COMPILER_SUPPORTS_TEMPLATE_TEMPLATES
                    .register_containers<long, std::set, std::vector>()
                    #else
                    .register_container<std::set<long>>()
                    .register_container<std::vector<long>>()
                    #endif
                    .compose_checked(formats::list { formats::defaults() })
                ;

    person p("Bob", "Builder", 20, std::set<long>(), { 1 });
    value expected = object({ { "firstname", p.firstname },
                              { "lastname",  p.lastname  },
                              { "winning_numbers", array({ 1 }) },
                            }
                           );
    value encoded = to_json(p, fmt);
    ensure_eq(expected, encoded);
}

TEST(serialization_builder_check_references_fails)
{
    formats_builder builder;
    builder.reference_type(std::type_index(typeid(int)));
    builder.reference_type(std::type_index(typeid(long)), std::type_index(typeid(person)));
    ensure_throws(std::logic_error, builder.check_references(formats(), "test"));
}

namespace
{

struct foo
{
    int         a;
    int         b;
    std::string c;

    static foo create_default()
    {
        foo out;
        out.a = 0;
        out.b = 1;
        out.c = "default";
        return out;
    }
};

struct bar
{
    foo         x;
    foo         y;
    std::string z;
    std::string w;
};

TEST(serialization_builder_extra_unchecked_key)
{
    jsonv::formats local_formats =
        jsonv::formats_builder()
            .type<foo>()
               .member("a", &foo::a)
               .member("b", &foo::b)
                   .default_value(10)
                   .default_on_null()
               .member("c", &foo::c)
            .type<bar>()
               .member("x", &bar::x)
               .member("y", &bar::y)
               .member("z", &bar::z)
                   .since(jsonv::version(2, 0))
               .member("w", &bar::w)
                   .until(jsonv::version(5, 0))
    ;
    jsonv::formats format = jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });

    jsonv::value val = object({ { "x", object({ { "a", 50 }, { "b", 20 }, { "c", "Blah"  }, {  "extra", "key" } }) },
                                { "y", object({ { "a", 10 },              { "c", "No B?" } }) },
                                { "z", "Only serialized in 2.0+" },
                                { "w", "Only serialized before 5.0" }
                              }
                             );
    bar x = jsonv::extract<bar>(val, format);
}

TEST(serialization_builder_extra_unchecked_key_throws)
{
    jsonv::formats local_formats =
        jsonv::formats_builder()
            .type<foo>()
               .on_extract_extra_keys(jsonv::throw_extra_keys_extraction_error)
               .member("a", &foo::a)
               .member("b", &foo::b)
                   .default_value(10)
                   .default_on_null()
               .member("c", &foo::c)
            .type<bar>()
               .member("x", &bar::x)
               .member("y", &bar::y)
               .member("z", &bar::z)
                   .since(jsonv::version(2, 0))
               .member("w", &bar::w)
                   .until(jsonv::version(5, 0))
    ;
    jsonv::formats format = jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });

    jsonv::value val = object({ { "x", object({ { "a", 50 }, { "b", 20 }, { "c", "Blah"  }, { "extra", "key" } }) },
                                { "y", object({ { "a", 10 },              { "c", "No B?" } }) },
                                { "z", "Only serialized in 2.0+" },
                                { "w", "Only serialized before 5.0" }
                              }
                             );
    try
    {
        jsonv::extract<bar>(val, format);
        throw std::runtime_error("Should have thrown an extraction_error");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(path({"x"}), err.path());
    }
}

TEST(serialization_builder_type_based_default_value)
{
    jsonv::formats local_formats =
        jsonv::formats_builder()
            .type<foo>()
                .member("a", &foo::a)
                .member("b", &foo::b)
                .member("c", &foo::c)
                .type_default_on_null()
                .type_default_value(foo::create_default())
        ;
    jsonv::formats format = jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });

    auto f = jsonv::extract<foo>(value(), format);
    auto e = foo::create_default();
    ensure_eq(e.a, f.a);
    ensure_eq(e.b, f.b);
    ensure_eq(e.c, f.c);
}

class wrapped_things
{
public:
    wrapped_things() = default;

    wrapped_things(int x, int y) :
            _x(x),
            _y(y)
    { }

    const int& x() const { return _x; }
    int&       x()       { return _x; }

    const int& y() const  { return _y; }
    void       y(int&& val) { _y = val; }

    friend bool operator==(const wrapped_things& a, const wrapped_things& b)
    {
        return a.x() == b.x()
            && a.y() == b.y();
    }

    friend std::ostream& operator<<(std::ostream& os, const wrapped_things& val)
    {
        return os << '(' << val.x() << ", " << val.y() << ')';
    }

private:
    int _x;
    int _y;
};

TEST(serialization_builder_access_mutate)
{
#ifndef _MSC_VER
    jsonv::formats local_formats =
        jsonv::formats_builder()
            .type<wrapped_things>()
                .member("x", &wrapped_things::x, &wrapped_things::x)
                .member("y", &wrapped_things::y, &wrapped_things::y)
        ;
    jsonv::formats format = jsonv::formats::compose({ jsonv::formats::defaults(), local_formats });
#endif
}

}

namespace x
{

enum class ring
{
    fire,
    wind,
    water,
    earth,
    heart,
};

}

TEST(serialization_builder_enum_strings)
{
    using namespace x;

    jsonv::formats formats =
        jsonv::formats_builder()
            .enum_type<ring>("ring",
                             {
                               { ring::fire,  "fire"  },
                               { ring::wind,  "wind"  },
                               { ring::water, "water" },
                               { ring::earth, "earth" },
                               { ring::heart, "heart" },
                             }
                            )
            #if JSONV_COMPILER_SUPPORTS_TEMPLATE_TEMPLATES
            .register_containers<ring, std::vector>()
            #else
            .register_container<std::vector<ring>>()
            #endif
            .check_references();

    ensure(ring::fire  == jsonv::extract<ring>("fire",  formats));
    ensure(ring::wind  == jsonv::extract<ring>("wind",  formats));
    ensure(ring::water == jsonv::extract<ring>("water", formats));
    ensure(ring::earth == jsonv::extract<ring>("earth", formats));
    ensure(ring::heart == jsonv::extract<ring>("heart", formats));

    jsonv::value jsons = jsonv::array({ "fire", "wind", "water", "earth", "heart" });
    std::vector<ring> exp = { ring::fire, ring::wind, ring::water, ring::earth, ring::heart };
    std::vector<ring> val = jsonv::extract<std::vector<ring>>(jsons, formats);
    ensure(val == exp);

    value enc = jsonv::to_json(exp, formats);
    ensure(enc == jsons);

    ensure_throws(jsonv::extraction_error, jsonv::extract<ring>("FIRE",    formats));
    ensure_throws(jsonv::extraction_error, jsonv::extract<ring>("useless", formats));
}

TEST(serialization_builder_enum_strings_icase)
{
    using namespace x;

    jsonv::formats formats =
        jsonv::formats_builder()
            .enum_type_icase<ring>("ring",
                             {
                               { ring::fire,  "fire"  },
                               { ring::wind,  "wind"  },
                               { ring::water, "water" },
                               { ring::earth, "earth" },
                               { ring::heart, "heart" },
                             }
                            )
            #if JSONV_COMPILER_SUPPORTS_TEMPLATE_TEMPLATES
            .register_containers<ring, std::vector>()
            #else
            .register_container<std::vector<ring>>()
            #endif
            .check_references(jsonv::formats::defaults());

    ensure(ring::fire  == jsonv::extract<ring>("fiRe",  formats));
    ensure(ring::wind  == jsonv::extract<ring>("wIND",  formats));
    ensure(ring::water == jsonv::extract<ring>("Water", formats));
    ensure(ring::earth == jsonv::extract<ring>("EARTH", formats));
    ensure(ring::heart == jsonv::extract<ring>("HEART", formats));

    jsonv::value jsons = jsonv::array({ "fire", "wind", "water", "earth", "heart" });
    std::vector<ring> exp = { ring::fire, ring::wind, ring::water, ring::earth, ring::heart };
    std::vector<ring> val = jsonv::extract<std::vector<ring>>(jsons, formats);
    ensure(val == exp);

    value enc = jsonv::to_json(exp, formats);
    ensure(enc == jsons);

    ensure_throws(jsonv::extraction_error, jsonv::extract<ring>("useless", formats));
}

TEST(serialization_builder_enum_strings_icase_multimapping)
{
    using namespace x;

    jsonv::formats formats =
        jsonv::formats_builder()
            .enum_type_icase<ring>("ring",
                             {
                               { ring::fire,  "fire"  },
                               { ring::fire,  666     },
                               { ring::wind,  "wind"  },
                               { ring::water, "water" },
                               { ring::earth, "earth" },
                               { ring::earth, true    },
                               { ring::heart, "heart" },
                               { ring::heart, "useless" },
                             }
                            )
             #if JSONV_COMPILER_SUPPORTS_TEMPLATE_TEMPLATES
             .register_containers<ring, std::vector>()
             #else
             .register_container<std::vector<ring>>()
             #endif
            .check_references(jsonv::formats::defaults());

    ensure(ring::fire  == jsonv::extract<ring>("fiRe",  formats));
    ensure(ring::fire  == jsonv::extract<ring>(666,     formats));
    ensure(ring::wind  == jsonv::extract<ring>("wIND",  formats));
    ensure(ring::water == jsonv::extract<ring>("Water", formats));
    ensure(ring::earth == jsonv::extract<ring>("EARTH", formats));
    ensure(ring::earth == jsonv::extract<ring>(true, formats));
    ensure(ring::heart == jsonv::extract<ring>("HEART", formats));
    ensure(ring::heart == jsonv::extract<ring>("useless", formats));

    jsonv::value jsons = jsonv::array({ "fire", "wind", "water", "earth", "heart" });
    std::vector<ring> exp = { ring::fire, ring::wind, ring::water, ring::earth, ring::heart };
    std::vector<ring> val = jsonv::extract<std::vector<ring>>(jsons, formats);
    ensure(val == exp);

    value enc = jsonv::to_json(exp, formats);
    ensure(enc == jsons);

    ensure_throws(jsonv::extraction_error, jsonv::extract<ring>(false, formats));
    ensure_throws(jsonv::extraction_error, jsonv::extract<ring>(5,     formats));
}

namespace
{

struct base
{
    virtual std::string get() const = 0;
};

struct a_derived :
        base
{
    virtual std::string get() const override { return "a"; }

    static void json_adapt(adapter_builder<a_derived>& builder)
    {
        builder.member("type", &a_derived::x);
    }

    std::string x = "a";
};

struct b_derived :
        base
{
    virtual std::string get() const override { return "b"; }

    static void json_adapt(adapter_builder<b_derived>& builder)
    {
        builder.member("type", &b_derived::x);
    }

    static void json_adapt_bad_value(adapter_builder<b_derived>& builder)
    {
        builder.member("type", &b_derived::bad);
    }

    static void json_adapt_no_value(adapter_builder<b_derived>&)
    {
    }

    std::string x = "b";
    std::string bad = "bad";
};

struct c_derived :
        base
{
    virtual std::string get() const override { return "c"; }

    static void json_adapt(adapter_builder<c_derived>&)
    {
    }

    static void json_adapt_some_value(adapter_builder<c_derived>& builder)
    {
        builder.member("type", &c_derived::x);
    }

    std::string x = "c";
};

}

TEST(serialization_builder_polymorphic_direct)
{
    auto make_fmts = [](std::function<void(adapter_builder<b_derived>&)> b_adapter,
                        std::function<void(adapter_builder<c_derived>&)> c_adapter)
                     {
                         return formats::compose
                                ({
                                    formats_builder()
                                        .polymorphic_type<std::unique_ptr<base>>("type")
                                            .subtype<a_derived>("a")
                                            .subtype<b_derived>("b", keyed_subtype_action::check)
                                            .subtype<c_derived>("c", keyed_subtype_action::insert)
                                        .type<a_derived>(a_derived::json_adapt)
                                        .type<b_derived>(b_adapter)
                                        .type<c_derived>(c_adapter)
                                        .register_container<std::vector<std::unique_ptr<base>>>()
                                        .check_references(formats::defaults()),
                                    formats::defaults()
                                });
                     };

    auto make_bad_fmts = []()
                         {
                             formats_builder()
                                 .polymorphic_type<std::unique_ptr<base>>("type")
                                     .subtype<a_derived>("a")
                                     .subtype<a_derived>("a");
                         };

    auto fmts = make_fmts(b_derived::json_adapt, c_derived::json_adapt);
    value input = array({ object({{ "type", "a" }}), object({{ "type", "b" }}), object({{ "type", "c" }}) });
    auto output = extract<std::vector<std::unique_ptr<base>>>(input, fmts);

    ensure(output.at(0)->get() == "a");
    ensure(output.at(1)->get() == "b");
    ensure(output.at(2)->get() == "c");

    value encoded = to_json(output, fmts);
    ensure_eq(input, encoded);

    // If the b type serializes the wrong value for "type" we should get a runtime_error.
    fmts = make_fmts(b_derived::json_adapt_bad_value, c_derived::json_adapt);
    ensure_throws(std::runtime_error, to_json(output, fmts));

    // If the b type does not add a "type" key we should get a runtime_error.
    fmts = make_fmts(b_derived::json_adapt_no_value, c_derived::json_adapt);
    ensure_throws(std::runtime_error, to_json(output, fmts));

    // If the c type serializes "type" at all we should get an error, because we expected to insert it ourselves.
    fmts = make_fmts(b_derived::json_adapt, c_derived::json_adapt_some_value);
    ensure_throws(std::runtime_error, to_json(output, fmts));

    // Attempting to register the same keyed subtype twice should result in a duplicate_type_error.
    ensure_throws(duplicate_type_error, make_bad_fmts());
}

TEST(serialization_builder_duplicate_type_actions)
{
    // Make one adapter that serializes and deserializes an int directly.
    static const auto adapter1 = make_adapter(
        [](const extraction_context&, const value& v) { return sometype{v.as_integer()}; },
        [](const serialization_context&, const sometype& v) { return value(v.v); });

    // Make another adapter that adds one each time an int is serialized and deserialized.
    static const auto adapter2 = make_adapter(
        [](const extraction_context&, const value& v) { return sometype{v.as_integer() + 1}; },
        [](const serialization_context&, const sometype& v) { return value(v.v + 1); });

    // Helper that serializes then deserializes an integer and returns the result.
    static const auto serde = [] (int v, const formats& f) { return extract<sometype>(to_json(sometype{v}, f), f).v; };

    // Initial adapter registration.
    formats_builder builder;
    builder.register_adapter(&adapter1);
    ensure_eq(1, serde(1, builder));

    // Default should throw, and the adapter should be unchanged.
    ensure_throws(duplicate_type_error, builder.register_adapter(&adapter2));
    ensure_eq(1, serde(1, builder));

    // Ignore should not throw, but the adapter should still be unchanged.
    builder.on_duplicate_type(duplicate_type_action::ignore);
    builder.register_adapter(&adapter2);
    ensure_eq(1, serde(1, builder));

    // Replace should not throw, and the adapter should be replaced.
    builder.on_duplicate_type(duplicate_type_action::replace);
    builder.register_adapter(&adapter2);
    ensure_eq(3, serde(1, builder));

    // Going back to exception should again throw an exception.
    builder.on_duplicate_type(duplicate_type_action::exception);
    ensure_throws(duplicate_type_error, builder.register_adapter(&adapter1));
    ensure_eq(3, serde(1, builder));
}

}
