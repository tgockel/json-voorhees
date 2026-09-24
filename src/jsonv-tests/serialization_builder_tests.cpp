/// \file
///
/// Copyright (c) 2015-2019 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/parse.hpp>
#include <jsonv/serialization_builder.hpp>
#include <jsonv/serialization/function_adapter.hpp>

#include <array>
#include <optional>
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
           std::optional<std::string> m = std::nullopt
          ) :
            firstname(std::move(f)),
            middle_name(m),
            lastname(std::move(l)),
            age(a),
            favorite_numbers(std::move(favorite_numbers)),
            winning_numbers(std::move(winning_numbers))
    { }

    std::string           firstname;
    std::optional<std::string> middle_name;
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
                        .member("firstname",   &person::firstname)
                        .member("middle_name", &person::middle_name)
                        .member("lastname",    &person::lastname)
                        .member("age",         &person::age)
                        .register_optional<std::optional<std::string>>()
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
                    .register_containers<long, std::set, std::vector>()
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
    auto extra_keys_handler = [&extra_keys] (extraction_context&, std::set<std::string> x)
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
                            .default_value(std::vector<long>())
                            .default_on_null()
                        // A default computed from a sibling member cannot be a `default_value` any more: the walk is
                        // a forward one, so when a missing key is noticed the object it would have read from has
                        // already gone by. `post_extract` sees the whole object and is where such a default belongs.
                        .post_extract([] (extraction_context&, person&& out) -> person
                                      {
                                          if (out.winning_numbers.empty())
                                              out.winning_numbers.assign(begin(out.favorite_numbers),
                                                                         end(out.favorite_numbers)
                                                                        );
                                          return std::move(out);
                                      }
                                     )
                    .register_containers<long, std::set, std::vector>()
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

TEST(serialization_builder_defaults_are_taken_from_a_document_missing_them)
{
    // The test above round-trips an object which has every key, so it never reaches a default at all. This one reads
    // the document that one builds and discards: `age` is absent and `winning_numbers` is null, which is the pair of
    // paths `default_value` and `default_on_null` exist for. The sibling-derived half of it is a `post_extract`,
    // which is where such a default has to live now that the walk is a forward one.
    formats fmt = formats_builder()
                    .type<person>()
                        .member("firstname",        &person::firstname)
                        .member("lastname",         &person::lastname)
                        .member("age",              &person::age)
                            .default_value(20)
                        .member("favorite_numbers", &person::favorite_numbers)
                        .member("winning_numbers",  &person::winning_numbers)
                            .default_value(std::vector<long>())
                            .default_on_null()
                        .post_extract([] (extraction_context&, person&& out) -> person
                                      {
                                          if (out.winning_numbers.empty())
                                              out.winning_numbers.assign(begin(out.favorite_numbers),
                                                                         end(out.favorite_numbers)
                                                                        );
                                          return std::move(out);
                                      }
                                     )
                    .register_containers<long, std::set, std::vector>()
                    .compose_checked(formats::defaults())
                ;

    value input = object({ { "firstname",        "Bob"                },
                           { "lastname",         "Builder"            },
                           { "favorite_numbers", array({ 1, 2, 3, 4 })},
                           { "winning_numbers",  null                 },
                         }
                        );

    person q = extract<person>(input, fmt);
    ensure_eq(person("Bob", "Builder", 20, { 1, 2, 3, 4 }, { 1, 2, 3, 4 }), q);
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
                    .register_containers<long, std::set, std::vector>()
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
        (void) jsonv::extract<bar>(val, format);
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
            .register_containers<ring, std::vector>()
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
            .register_containers<ring, std::vector>()
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
            .register_containers<ring, std::vector>()
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
    virtual ~base() = default;

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


namespace
{

/// Three required members with short names, for walking the key loop over.
struct triple
{
    std::int64_t a;
    std::int64_t b;
    std::int64_t c;

    bool operator==(const triple& other) const
    {
        return a == other.a && b == other.b && c == other.c;
    }

    friend std::ostream& operator<<(std::ostream& os, const triple& x)
    {
        return os << "{ " << x.a << ", " << x.b << ", " << x.c << " }";
    }

    friend std::string to_string(const triple& x)
    {
        std::ostringstream os;
        os << x;
        return os.str();
    }
};

formats triple_formats()
{
    static const formats instance = formats_builder()
                                        .type<triple>()
                                            .member("a", &triple::a)
                                            .member("b", &triple::b)
                                            .member("c", &triple::c)
                                    .compose_checked(formats::defaults());

    return instance;
}

/// A reader over \a source, stepped off `document_start` and onto the value itself.
///
/// The DSL tests mostly extract from a `jsonv::value`, which is the shorter spelling -- but a value has already had
/// its duplicate keys collapsed and spells every key canonically, so the two things only a *text* source can present
/// have to be read as text.
reader open(std::string_view source)
{
    reader out(source);
    (void) out.next_token();
    return out;
}

extract_options collecting(extract_options::size_type max_failures = 10U)
{
    return extract_options::create_default()
                .failure_mode(extract_options::on_error::collect_all)
                .max_failures(max_failures);
}

}

TEST(serialization_builder_skips_an_unknown_nested_key)
{
    // An unrecognised key is stepped over with `reader::next_value`, which crosses the whole subtree in one move on a
    // tape-backed reader rather than walking into it. The member after it is what proves the cursor landed in the
    // right place: leaving the walk inside `ignored` would read its members as this object's.
    extraction_context cxt(triple_formats());
    auto               rdr = open(R"({
                                       "a": 1,
                                       "ignored": { "a": 90, "deeper": [ 1, 2, { "b": 3 }, [ [ [ 4 ] ] ] ] },
                                       "b": 2,
                                       "c": 3
                                     })");

    auto out = cxt.extract<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);
}

TEST(serialization_builder_skips_an_unknown_trailing_key)
{
    // The last key in the object claims no member, so the step over it has to land on the `}` rather than past it.
    ensure_eq(triple({ 1, 2, 3 }),
              extract<triple>(parse(R"({ "a": 1, "b": 2, "c": 3, "extra": [ 1, 2, 3 ] })"), triple_formats())
             );
}

TEST(serialization_builder_skips_an_only_unknown_key)
{
    // Nothing claims anything, so every member falls to the pass over the ones no key claimed -- and all three are
    // required.
    try
    {
        (void) extract<triple>(parse(R"({ "extra": 1 })"), triple_formats(), collecting());
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(3U, err.problems().size());
        ensure_eq(std::string("Missing required field a"), err.problems().at(0).message());
        ensure_eq(std::string("Missing required field c"), err.problems().at(2).message());
    }
}

TEST(serialization_builder_members_in_any_document_order)
{
    // Declaration order is what the members are *reported* in; it is not the order they are read in any more.
    ensure_eq(triple({ 1, 2, 3 }),
              extract<triple>(parse(R"({ "c": 3, "a": 1, "b": 2 })"), triple_formats())
             );
}

TEST(serialization_builder_empty_object_takes_every_default)
{
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .default_value(7)
                        .member("b", &triple::b)
                            .default_value(8)
                        .member("c", &triple::c)
                            .default_value(9)
                  .compose_checked(formats::defaults());

    ensure_eq(triple({ 7, 8, 9 }), extract<triple>(parse("{}"), fmt));
}

TEST(serialization_builder_empty_object_still_requires_a_member_without_one)
{
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .default_value(7)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                            .default_value(9)
                  .compose_checked(formats::defaults());

    try
    {
        (void) extract<triple>(parse("{}"), fmt);
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure_eq(std::string("Missing required field b"), err.problems().at(0).message());
    }
}

TEST(serialization_builder_alternate_name_prefers_the_declared_one)
{
    // Both spellings are in the document. The declared name is preferred whichever order they arrive in, which the
    // walk has to decide for itself: it meets the names in the order the document put them, and that says nothing
    // about which one the type prefers. A key which loses that race is not an extra key either -- a member answers
    // for every name it has.
    //
    // Read as text on purpose. A `jsonv::value` is a sorted map, so extracting one only ever presents the two
    // spellings in collating order and an implementation which took the last of them would pass half the time by
    // accident.
    std::set<std::string> extra_keys;

    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alternate_name("A")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                        .on_extract_extra_keys([&extra_keys] (extraction_context&, std::set<std::string> found)
                                               {
                                                   extra_keys = std::move(found);
                                               }
                                              )
                  .compose_checked(formats::defaults());

    for (std::string_view source : { R"({ "A": 50, "a": 1, "b": 2, "c": 3 })",
                                     R"({ "a": 1, "A": 50, "b": 2, "c": 3 })"
                                   })
    {
        extraction_context cxt(fmt);
        auto               rdr = open(source);

        auto out = cxt.extract<triple>(rdr);
        ensure(out.has_value());
        ensure(cxt.problems().empty());
        ensure_eq(triple({ 1, 2, 3 }), *out);
    }

    ensure_eq(triple({ 1, 2, 3 }), extract<triple>(parse(R"({ "A": 50, "a": 1, "b": 2, "c": 3 })"), fmt));
    ensure(extra_keys.empty());
}

TEST(serialization_builder_alternate_names_rank_against_each_other)
{
    // Two alternates, so the choice is not simply "the declared one or not". They are preferred in the order they
    // were added, again whichever order the document uses.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alternate_name("first_alternate")
                            .alternate_name("second_alternate")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    for (std::string_view source : { R"({ "first_alternate": 1, "second_alternate": 50, "b": 2, "c": 3 })",
                                     R"({ "second_alternate": 50, "first_alternate": 1, "b": 2, "c": 3 })"
                                   })
    {
        extraction_context cxt(fmt);
        auto               rdr = open(source);

        auto out = cxt.extract<triple>(rdr);
        ensure(out.has_value());
        ensure(cxt.problems().empty());
        ensure_eq(triple({ 1, 2, 3 }), *out);
    }
}

TEST(serialization_builder_an_alternate_name_is_not_a_duplicate_key)
{
    // Naming one member two ways and repeating one key are different things, and only the second is
    // `duplicate_key_action`'s to refuse. Strict handling used to reject this document, which is valid.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alternate_name("A")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    auto strict = extract_options::create_default()
                      .on_duplicate_key(extract_options::duplicate_key_action::exception);

    extraction_context cxt(fmt, std::nullopt, jsonv::path(), nullptr, strict);
    auto               rdr = open(R"({ "a": 1, "A": 50, "b": 2, "c": 3 })");

    auto out = cxt.extract<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);

    // The same name twice still is one.
    extraction_context repeated(fmt, std::nullopt, jsonv::path(), nullptr, strict);
    auto               repeated_rdr = open(R"({ "a": 1, "a": 50, "b": 2, "c": 3 })");

    ensure(!repeated.extract<triple>(repeated_rdr).has_value());
    ensure_eq(std::string("Duplicate key in object: \"a\""), repeated.problems().at(0).message());
}

TEST(serialization_builder_a_superseded_alternate_is_not_validated)
{
    // The spelling which loses is stepped over unread, so a `check_input` on the member never sees it. Reading it
    // only to throw the result away would report failures against a value the type did not take.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alternate_name("A")
                            .check_input([] (const std::int64_t& value)
                                         {
                                             if (value > 10)
                                                 throw std::logic_error("a must be small");
                                         }
                                        )
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    extraction_context cxt(fmt);
    auto               rdr = open(R"({ "a": 1, "A": 5000, "b": 2, "c": 3 })");

    auto out = cxt.extract<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);
}

TEST(serialization_builder_names_the_key_the_document_used)
{
    // A failure inside a member is reported at the key the *document* spelled, not at the name the member was
    // declared with -- which is what makes `alternate_name` a matching rule rather than a rename.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alternate_name("A")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    try
    {
        (void) extract<triple>(parse(R"({ "A": "not a number", "b": 2, "c": 3 })"), fmt);
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(path::create(".A"), err.path());
    }
}

TEST(serialization_builder_matches_an_escaped_key)
{
    // A key the document spelled with escape sequences arrives as `ast_node_type::key_escaped` and has to be decoded
    // before it can be matched. Only a text source produces one: a `jsonv::value` has already decoded its keys.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a\nb", &triple::a)
                        .member("b",    &triple::b)
                        .member("c",    &triple::c)
                  .compose_checked(formats::defaults());

    extraction_context cxt(fmt);
    auto               rdr = open(R"({ "a\nb": 1, "b": 2, "c": 3 })");

    auto out = cxt.extract<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);
}

TEST(serialization_builder_names_an_escaped_key_it_failed_in)
{
    // The decoded key outlives the member's extraction, so it is still there to name the failure.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a\nb", &triple::a)
                        .member("b",    &triple::b)
                        .member("c",    &triple::c)
                  .compose_checked(formats::defaults());

    extraction_context cxt(fmt);
    auto               rdr = open(R"({ "a\nb": "not a number", "b": 2, "c": 3 })");

    ensure(!cxt.extract<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    // Spelled as an element rather than through `path::create`, which parses its argument and has no syntax for a
    // key containing a newline.
    ensure_eq(path({ "a\nb" }), cxt.problems().at(0).path());
}

TEST(serialization_builder_duplicate_key_replaces_by_default)
{
    // `extract_options::duplicate_key_action::replace` is the default and means the last spelling of a key wins,
    // which is what `parse_index::extract_tree` does with one.
    extraction_context cxt(triple_formats());
    auto               rdr = open(R"({ "a": 1, "b": 2, "c": 3, "a": 99 })");

    auto out = cxt.extract<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 99, 2, 3 }), *out);
}

TEST(serialization_builder_duplicate_key_can_keep_the_first)
{
    extraction_context cxt(triple_formats(),
                           std::nullopt,
                           jsonv::path(),
                           nullptr,
                           extract_options::create_default()
                               .on_duplicate_key(extract_options::duplicate_key_action::ignore)
                          );
    auto rdr = open(R"({ "a": 1, "b": 2, "c": 3, "a": 99 })");

    auto out = cxt.extract<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);
}

TEST(serialization_builder_duplicate_key_can_be_refused)
{
    extraction_context cxt(triple_formats(),
                           std::nullopt,
                           jsonv::path(),
                           nullptr,
                           extract_options::create_default()
                               .on_duplicate_key(extract_options::duplicate_key_action::exception)
                          );
    auto rdr = open(R"({ "a": 1, "b": 2, "c": 3, "a": 99 })");

    ensure(!cxt.extract<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Duplicate key in object: \"a\""), cxt.problems().at(0).message());
}

TEST(serialization_builder_refuses_a_non_object)
{
    // The walk asks for `{` and says what it found instead. This used to be a `kind_error` out of `value::find`,
    // because each member looked itself up in something which was not an object.
    extraction_context cxt(triple_formats());
    auto               rdr = open("5");

    ensure(!cxt.extract<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure(cxt.problems().at(0).message().find("object") != std::string::npos);
}

TEST(serialization_builder_check_input_rejects_a_member)
{
    // `check_input` runs on the value which was read, before it reaches the member. It had never run at all: the
    // mutator it composes into was stored and never called.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .check_input([] (const std::int64_t& value)
                                         {
                                             if (value < 0)
                                                 throw std::logic_error("a must not be negative");
                                         }
                                        )
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    ensure_eq(triple({ 1, 2, 3 }), extract<triple>(parse(R"({ "a": 1, "b": 2, "c": 3 })"), fmt));

    try
    {
        (void) extract<triple>(parse(R"({ "a": -1, "b": 2, "c": 3 })"), fmt);
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure(err.problems().at(0).nested_ptr());
        ensure_throws(std::logic_error, (std::rethrow_exception(err.problems().at(0).nested_ptr()), 0));
    }
}


TEST(serialization_builder_more_members_than_a_word_has_bits)
{
    // Which members a key has claimed is tracked in a machine word while there is room and spills to a heap-allocated
    // set when there is not, so the handover is worth walking over. The members here are deliberately declared in an
    // order the document does not use, and every one past the inline capacity has a default, so both halves of the
    // post-walk pass run on the spilled side.
    constexpr std::size_t count = 70U;

    struct wide
    {
        std::array<std::int64_t, count> v{};
    };

    formats_builder builder;
    auto            type_builder = builder.type<wide>();
    for (std::size_t idx = 0U; idx < count; ++idx)
    {
        auto member = type_builder.template member<std::int64_t>(
            "m" + std::to_string(idx),
            std::function<const std::int64_t& (const wide&)>([idx] (const wide& x) -> const std::int64_t&
                                                             {
                                                                 return x.v[idx];
                                                             }),
            std::function<void (wide&, std::int64_t&&)>([idx] (wide& x, std::int64_t&& val)
                                                        {
                                                            x.v[idx] = val;
                                                        })
        );

        // Everything from the inline capacity on is optional, so the document can leave it out.
        if (idx >= 64U)
            member.default_value(std::int64_t(-1));
    }

    formats fmt = builder.compose_checked(formats::defaults());

    // Written back to front, and stopping short of the members which have defaults.
    value source = object();
    for (std::size_t idx = 66U; idx-- > 0U; )
        source["m" + std::to_string(idx)] = value(std::int64_t(idx) * 10);

    wide out = extract<wide>(source, fmt);
    for (std::size_t idx = 0U; idx < 66U; ++idx)
        ensure_eq(std::int64_t(idx) * 10, out.v[idx]);
    for (std::size_t idx = 66U; idx < count; ++idx)
        ensure_eq(std::int64_t(-1), out.v[idx]);

    // And the same walk still notices one of the spilled members going missing when it has nothing to fall back on.
    formats_builder strict_builder;
    auto            strict_type = strict_builder.type<wide>();
    for (std::size_t idx = 0U; idx < count; ++idx)
    {
        strict_type.template member<std::int64_t>(
            "m" + std::to_string(idx),
            std::function<const std::int64_t& (const wide&)>([idx] (const wide& x) -> const std::int64_t&
                                                             {
                                                                 return x.v[idx];
                                                             }),
            std::function<void (wide&, std::int64_t&&)>([idx] (wide& x, std::int64_t&& val)
                                                        {
                                                            x.v[idx] = val;
                                                        })
        );
    }

    try
    {
        (void) extract<wide>(source, strict_builder.compose_checked(formats::defaults()));
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        ensure_eq(std::string("Missing required field m66"), err.problems().at(0).message());
    }
}


TEST(serialization_builder_a_failed_type_default_does_not_eat_the_next_value)
{
    // `type_default_on_null` consumes the `null` before the factory runs, so a factory which throws fails with the
    // value it stood in for already behind the cursor. Saying so is what stops the array above stepping over the
    // element *after* it -- which would drop that element from the result and every problem it had to report.
    formats fmt = formats::compose({ formats_builder()
                                         .type<triple>()
                                             .member("a", &triple::a)
                                             .member("b", &triple::b)
                                             .member("c", &triple::c)
                                             .type_default_on_null()
                                             .type_default_value([] (extraction_context&) -> triple
                                                                 {
                                                                     throw std::runtime_error("no default to give");
                                                                 }
                                                                )
                                         .register_container<std::vector<triple>>()
                                     .compose_checked(formats::defaults())
                                   });

    try
    {
        (void) extract<std::vector<triple>>(parse(R"([ null, { "a": "bad", "b": 2, "c": 3 } ])"),
                                            fmt,
                                            collecting()
                                           );
        ensure(!"extraction_error was not thrown");
    }
    catch (const extraction_error& err)
    {
        // The factory's failure at `[0]`, and then the element after it -- not just the first.
        ensure_eq(2U, err.problems().size());
        ensure_eq(path::create("[0]"),    err.problems().at(0).path());
        ensure_eq(path::create("[1].a"),  err.problems().at(1).path());
    }
}


TEST(serialization_builder_strict_duplicates_do_not_depend_on_key_order)
{
    // A second helping of a name the member has already passed over for a better one is still a repeat, but the
    // winning name cannot see that: a lower-ranked key looks the same whether it is a first sighting of one
    // alternate or a second of another. Strict handling therefore asks of the keys themselves, so the same object is
    // refused whichever order it listed them in.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alternate_name("A")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    auto strict = extract_options::create_default()
                      .on_duplicate_key(extract_options::duplicate_key_action::exception);

    for (std::string_view source : { R"({ "A": 1, "a": 2, "A": 3, "b": 2, "c": 3 })",
                                     R"({ "A": 1, "A": 3, "a": 2, "b": 2, "c": 3 })"
                                   })
    {
        extraction_context cxt(fmt, std::nullopt, jsonv::path(), nullptr, strict);
        auto               rdr = open(source);

        ensure(!cxt.extract<triple>(rdr).has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_eq(std::string("Duplicate key in object: \"A\""), cxt.problems().at(0).message());
    }
}

TEST(serialization_builder_strict_duplicates_cover_unrecognised_keys)
{
    // The policy is about the document repeating a key, which has nothing to do with whether this type happens to
    // want it -- and it is what `parse_index::extract_tree` refuses for the same document.
    auto strict = extract_options::create_default()
                      .on_duplicate_key(extract_options::duplicate_key_action::exception);

    extraction_context cxt(triple_formats(), std::nullopt, jsonv::path(), nullptr, strict);
    auto               rdr = open(R"({ "a": 1, "b": 2, "c": 3, "extra": 1, "extra": 2 })");

    ensure(!cxt.extract<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Duplicate key in object: \"extra\""), cxt.problems().at(0).message());
}

}
