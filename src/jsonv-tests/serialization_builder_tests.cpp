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
#include <jsonv/path.hpp>
#include <jsonv/serialization_builder.hpp>
#include <jsonv/serialization/function_adapter.hpp>
#include <jsonv/serialization/function_deserializer.hpp>
#include <jsonv/writer.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
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

/// The text \a from serializes to through \a context: written into a \c writer over a stream, which is where the
/// order the DSL writes members in can be seen. A \c value built by \c to_json sorts them.
template <typename T>
std::string serialize_to_text(const serialization_context& context, const T& from)
{
    std::ostringstream os;
    writer             to(os);
    context.serialize(from, to);
    return std::move(os).str();
}

template <typename T>
std::string serialize_to_text(const formats& fmts, const T& from)
{
    return serialize_to_text(serialization_context(fmts), from);
}

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

    person q = deserialize<person>(encoded, fmt);
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

    // Written directly, the member is left out key and all.
    auto text_ver = [&fmt] (const version& v)
                    {
                        return serialize_to_text(serialization_context(fmt, v), my_pair(5, 10));
                    };

    ensure_eq(std::string(R"({"a":5})"),        text_ver({ 1, 0 }));
    ensure_eq(std::string(R"({"a":5,"b":10})"), text_ver({ 2, 0 }));
    ensure_eq(std::string(R"({"a":5,"b":10})"), text_ver({ 3, 0 }));
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
    person q = deserialize<person>(encoded, fmt);
    ensure_eq(p, q);
}

TEST(serialization_builder_unknown_members)
{
    std::set<std::string> unknown_members;
    auto unknown_members_handler = [&unknown_members] (deserialization_context&, std::set<std::string> x)
                                   {
                                       unknown_members = std::move(x);
                                   };

    formats fmt = formats_builder()
                    .type<person>()
                        .member("firstname", &person::firstname)
                        .member("lastname",  &person::lastname)
                        .member("age",       &person::age)
                        .on_unknown_members(unknown_members_handler)
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

    person q = deserialize<person>(encoded, fmt);
    ensure_eq(p, q);
    ensure(unknown_members == std::set<std::string>({ "extra1", "extra2" }));
}

TEST(serialization_builder_post_deserialize_single)
{
    auto post_deserialize_handler = [] (const deserialization_context&, person&& p) -> person
                                  {
                                      p.age++;
                                      return p;
                                  };

    formats fmt = formats_builder()
                    .type<person>()
                        .post_deserialize(post_deserialize_handler)
                        .member("firstname", &person::firstname)
                        .member("lastname",  &person::lastname)
                        .member("age",       &person::age)
                    .compose_checked(formats::defaults())
                ;

    person p("Bob", "Builder", 29);
    auto encoded = to_json(p, fmt);
    person q = deserialize<person>(encoded, fmt);
    ensure_eq(person("Bob", "Builder", 30), q);
}

TEST(serialization_builder_post_deserialize_multi)
{
    auto post_deserialize_handler_1 = [] (const deserialization_context&, person&& p)
                                    {
                                        p.age++;
                                        return p;
                                    };

    auto post_deserialize_handler_2 = [] (const deserialization_context&, person&& p)
                                    {
                                        p.lastname = "Mc" + p.lastname;
                                        return p;
                                    };

    formats fmt = formats_builder()
                    .type<person>()
                        .post_deserialize(post_deserialize_handler_1)
                        .post_deserialize(post_deserialize_handler_2)
                        .member("firstname", &person::firstname)
                        .member("lastname",  &person::lastname)
                        .member("age",       &person::age)
                    .compose_checked(formats::defaults())
                ;

    person p("Bob", "Builder", 29);
    auto encoded = to_json(p, fmt);
    person q = deserialize<person>(encoded, fmt);
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
                            // Computed from a sibling, which the default can read because it is taken once the walk
                            // is done -- for the `null` below as much as for a key which never arrived.
                            .default_value([] (deserialization_context& cxt)
                                           {
                                               const value& source = cxt.source_value().value();
                                               return cxt.deserialize<std::vector<long>>(source.at("favorite_numbers"));
                                           }
                                          )
                            .default_on_null()
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
    ensure_eq(p, deserialize<person>(input, fmt));
    ensure_eq(p, deserialize<person>(std::string_view(to_string(input)), fmt));

    value missing = input;
    missing.erase("winning_numbers");
    ensure_eq(p, deserialize<person>(missing, fmt));
    ensure_eq(p, deserialize<person>(std::string_view(to_string(missing)), fmt));

    auto encoded = to_json(p, fmt);
    person q = deserialize<person>(encoded, fmt);
    ensure_eq(p, q);
}

TEST(serialization_builder_defaults_are_taken_from_a_document_missing_them)
{
    // The test above round-trips an object which has every key, so it never reaches a default at all. This one reads
    // the document that one builds and discards: `age` is absent and `winning_numbers` is null, which is the pair of
    // paths `default_value` and `default_on_null` exist for. The sibling-derived half of it is a `post_deserialize`,
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
                        .post_deserialize([] (deserialization_context&, person&& out) -> person
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

    person q = deserialize<person>(input, fmt);
    ensure_eq(person("Bob", "Builder", 20, { 1, 2, 3, 4 }, { 1, 2, 3, 4 }), q);
}

TEST(serialization_builder_serialize_checks)
{
    formats fmt = formats_builder()
                    .type<person>()
                        .member("firstname",        &person::firstname)
                        .member("lastname",         &person::lastname)
                        .member("age",              &person::age)
                            .serialize_if([] (const serialization_context&, int age) { return age > 20; })
                        .member("favorite_numbers", &person::favorite_numbers)
                            .serialize_if([] (const serialization_context&, const std::set<long>& nums)
                                          {
                                              return nums.size();
                                          }
                                         )
                        .member("winning_numbers",  &person::winning_numbers)
                            .serialize_if([] (const serialization_context&, const std::vector<long>& nums)
                                          {
                                              return nums.size();
                                          }
                                         )
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

    // Written directly, a member its check leaves out is left out key and all.
    ensure_eq(std::string(R"({"firstname":"Bob","lastname":"Builder","winning_numbers":[1]})"),
              serialize_to_text(fmt, p)
             );
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
    bar x = jsonv::deserialize<bar>(val, format);
}

TEST(serialization_builder_extra_unchecked_key_throws)
{
    jsonv::formats local_formats =
        jsonv::formats_builder()
            .type<foo>()
               .on_unknown_members(jsonv::deny_unknown_members)
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
        (void) jsonv::deserialize<bar>(val, format);
        throw std::runtime_error("Should have thrown a deserialization_error");
    }
    catch (const deserialization_error& err)
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

    auto f = jsonv::deserialize<foo>(value(), format);
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

    ensure(ring::fire  == jsonv::deserialize<ring>(jsonv::value("fire"),  formats));
    ensure(ring::wind  == jsonv::deserialize<ring>(jsonv::value("wind"),  formats));
    ensure(ring::water == jsonv::deserialize<ring>(jsonv::value("water"), formats));
    ensure(ring::earth == jsonv::deserialize<ring>(jsonv::value("earth"), formats));
    ensure(ring::heart == jsonv::deserialize<ring>(jsonv::value("heart"), formats));

    // A C++ string is JSON text, so the same string written as a document needs its quotes.
    ensure(ring::fire  == jsonv::deserialize<ring>(R"("fire")", formats));

    jsonv::value jsons = jsonv::array({ "fire", "wind", "water", "earth", "heart" });
    std::vector<ring> exp = { ring::fire, ring::wind, ring::water, ring::earth, ring::heart };
    std::vector<ring> val = jsonv::deserialize<std::vector<ring>>(jsons, formats);
    ensure(val == exp);

    value enc = jsonv::to_json(exp, formats);
    ensure(enc == jsons);

    ensure_throws(jsonv::deserialization_error, jsonv::deserialize<ring>(jsonv::value("FIRE"),    formats));
    ensure_throws(jsonv::deserialization_error, jsonv::deserialize<ring>(jsonv::value("useless"), formats));
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

    ensure(ring::fire  == jsonv::deserialize<ring>(jsonv::value("fiRe"),  formats));
    ensure(ring::wind  == jsonv::deserialize<ring>(jsonv::value("wIND"),  formats));
    ensure(ring::water == jsonv::deserialize<ring>(jsonv::value("Water"), formats));
    ensure(ring::earth == jsonv::deserialize<ring>(jsonv::value("EARTH"), formats));
    ensure(ring::heart == jsonv::deserialize<ring>(jsonv::value("HEART"), formats));

    jsonv::value jsons = jsonv::array({ "fire", "wind", "water", "earth", "heart" });
    std::vector<ring> exp = { ring::fire, ring::wind, ring::water, ring::earth, ring::heart };
    std::vector<ring> val = jsonv::deserialize<std::vector<ring>>(jsons, formats);
    ensure(val == exp);

    value enc = jsonv::to_json(exp, formats);
    ensure(enc == jsons);

    ensure_throws(jsonv::deserialization_error, jsonv::deserialize<ring>(jsonv::value("useless"), formats));
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

    ensure(ring::fire  == jsonv::deserialize<ring>(jsonv::value("fiRe"),  formats));
    ensure(ring::fire  == jsonv::deserialize<ring>(666,     formats));
    ensure(ring::wind  == jsonv::deserialize<ring>(jsonv::value("wIND"),  formats));
    ensure(ring::water == jsonv::deserialize<ring>(jsonv::value("Water"), formats));
    ensure(ring::earth == jsonv::deserialize<ring>(jsonv::value("EARTH"), formats));
    ensure(ring::earth == jsonv::deserialize<ring>(true, formats));
    ensure(ring::heart == jsonv::deserialize<ring>(jsonv::value("HEART"), formats));
    ensure(ring::heart == jsonv::deserialize<ring>(jsonv::value("useless"), formats));

    jsonv::value jsons = jsonv::array({ "fire", "wind", "water", "earth", "heart" });
    std::vector<ring> exp = { ring::fire, ring::wind, ring::water, ring::earth, ring::heart };
    std::vector<ring> val = jsonv::deserialize<std::vector<ring>>(jsons, formats);
    ensure(val == exp);

    value enc = jsonv::to_json(exp, formats);
    ensure(enc == jsons);

    ensure_throws(jsonv::deserialization_error, jsonv::deserialize<ring>(false, formats));
    ensure_throws(jsonv::deserialization_error, jsonv::deserialize<ring>(5,     formats));
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
    auto output = deserialize<std::vector<std::unique_ptr<base>>>(input, fmts);

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
        [](const deserialization_context&, const value& v) { return sometype{v.as_integer()}; },
        [](const serialization_context&, const sometype& v) { return value(v.v); });

    // Make another adapter that adds one each time an int is serialized and deserialized.
    static const auto adapter2 = make_adapter(
        [](const deserialization_context&, const value& v) { return sometype{v.as_integer() + 1}; },
        [](const serialization_context&, const sometype& v) { return value(v.v + 1); });

    // Helper that serializes then deserializes an integer and returns the result.
    static const auto serde = [] (int v, const formats& f)
                              {
                                  return deserialize<sometype>(to_json(sometype{v}, f), f).v;
                              };

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
/// The DSL tests mostly deserialize from a `jsonv::value`, which is the shorter spelling -- but a value has already had
/// its duplicate keys collapsed and spells every key canonically, so the two things only a *text* source can present
/// have to be read as text.
reader open(std::string_view source)
{
    reader out(source);
    (void) out.next_token();
    return out;
}

deserialize_options collecting(deserialize_options::size_type max_failures = 10U)
{
    return deserialize_options::create_default()
                .failure_mode(deserialize_options::on_error::collect_all)
                .max_failures(max_failures);
}

}

TEST(serialization_builder_skips_an_unknown_nested_key)
{
    // An unrecognised key is stepped over with `reader::next_value`, which crosses the whole subtree in one move on a
    // tape-backed reader rather than walking into it. The member after it is what proves the cursor landed in the
    // right place: leaving the walk inside `ignored` would read its members as this object's.
    deserialization_context cxt(triple_formats());
    auto               rdr = open(R"({
                                       "a": 1,
                                       "ignored": { "a": 90, "deeper": [ 1, 2, { "b": 3 }, [ [ [ 4 ] ] ] ] },
                                       "b": 2,
                                       "c": 3
                                     })");

    auto out = cxt.deserialize<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);
}

TEST(serialization_builder_skips_an_unknown_trailing_key)
{
    // The last key in the object claims no member, so the step over it has to land on the `}` rather than past it.
    ensure_eq(triple({ 1, 2, 3 }),
              deserialize<triple>(parse(R"({ "a": 1, "b": 2, "c": 3, "extra": [ 1, 2, 3 ] })"), triple_formats())
             );
}

TEST(serialization_builder_skips_an_only_unknown_key)
{
    // Nothing claims anything, so every member falls to the pass over the ones no key claimed -- and all three are
    // required.
    try
    {
        (void) deserialize<triple>(parse(R"({ "extra": 1 })"), triple_formats(), collecting());
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
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
              deserialize<triple>(parse(R"({ "c": 3, "a": 1, "b": 2 })"), triple_formats())
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

    ensure_eq(triple({ 7, 8, 9 }), deserialize<triple>(parse("{}"), fmt));
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
        (void) deserialize<triple>(parse("{}"), fmt);
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure_eq(1U, err.problems().size());
        ensure_eq(std::string("Missing required field b"), err.problems().at(0).message());
    }
}

TEST(serialization_builder_default_on_null_without_a_default_deserializes_the_null)
{
    // `default_on_null` is only considered when there is a default to take. Without one, a `null` is a value like any
    // other and goes to the member's own deserializer, which can say what is wrong with the document. It used to be
    // handed to the default factory nobody provided, and failed as a `std::bad_function_call`.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .default_on_null()
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    deserialization_context cxt(fmt);
    auto               rdr = open(R"({ "a": null, "b": 2, "c": 3 })");

    ensure(!cxt.deserialize<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Read node of type null when expecting integer"), cxt.problems().at(0).message());
    ensure_eq(path::create(".a"), cxt.problems().at(0).path());
    ensure(!cxt.problems().at(0).nested_ptr());

    // Nor does the flag make the member optional. A missing key is still a missing required field, which is the gated
    // branch the `null` one now matches.
    deserialization_context missing(fmt);
    auto               missing_rdr = open(R"({ "b": 2, "c": 3 })");

    ensure(!missing.deserialize<triple>(missing_rdr).has_value());
    ensure_eq(1U, missing.problems().size());
    ensure_eq(std::string("Missing required field a"), missing.problems().at(0).message());
}

TEST(serialization_builder_type_default_on_null_without_a_default_deserializes_the_null)
{
    // The type-level pair follows the same rule. With no `type_default_value`, a `null` is read like any other value,
    // and a type described by the DSL is read from an object.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                        .type_default_on_null()
                  .compose_checked(formats::defaults());

    deserialization_context cxt(fmt);
    auto               rdr = open("null");

    ensure(!cxt.deserialize<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Read node of type null when expecting object"), cxt.problems().at(0).message());
    ensure(!cxt.problems().at(0).nested_ptr());
}

TEST(serialization_builder_null_takes_the_default_only_when_asked)
{
    // A `null` takes the default only where both were asked for, in whichever order they were declared: the gate is
    // asked at deserialization time, not when the flag is set. A member with a `default_value` alone still reads an
    // explicit `null` as a value to deserialize, since its default is for a key which is missing.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .default_on_null()
                            .default_value(7)
                        .member("b", &triple::b)
                            .default_value(8)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    ensure_eq(triple({ 7, 2, 3 }), deserialize<triple>(parse(R"({ "a": null, "b": 2, "c": 3 })"), fmt));

    deserialization_context cxt(fmt);
    auto               rdr = open(R"({ "a": 1, "b": null, "c": 3 })");

    ensure(!cxt.deserialize<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Read node of type null when expecting integer"), cxt.problems().at(0).message());
    ensure_eq(path::create(".b"), cxt.problems().at(0).path());
}

TEST(serialization_builder_alias_prefers_the_declared_one)
{
    // Both spellings are in the document. The declared name is preferred whichever order they arrive in, which the
    // walk has to decide for itself: it meets the names in the order the document put them, and that says nothing
    // about which one the type prefers. A key which loses that race is not an unknown member either -- a member answers
    // for every name it has.
    //
    // Read as text on purpose. A `jsonv::value` is a sorted map, so deserializing one only ever presents the two
    // spellings in collating order and an implementation which took the last of them would pass half the time by
    // accident.
    std::set<std::string> unknown_members;

    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alias("A")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                        .on_unknown_members([&unknown_members] (deserialization_context&, std::set<std::string> found)
                                            {
                                                unknown_members = std::move(found);
                                            }
                                           )
                  .compose_checked(formats::defaults());

    for (std::string_view source : { R"({ "A": 50, "a": 1, "b": 2, "c": 3 })",
                                     R"({ "a": 1, "A": 50, "b": 2, "c": 3 })"
                                   })
    {
        deserialization_context cxt(fmt);
        auto               rdr = open(source);

        auto out = cxt.deserialize<triple>(rdr);
        ensure(out.has_value());
        ensure(cxt.problems().empty());
        ensure_eq(triple({ 1, 2, 3 }), *out);
    }

    ensure_eq(triple({ 1, 2, 3 }), deserialize<triple>(parse(R"({ "A": 50, "a": 1, "b": 2, "c": 3 })"), fmt));
    ensure(unknown_members.empty());
}

TEST(serialization_builder_aliases_rank_against_each_other)
{
    // Two aliases, so the choice is not simply "the declared one or not". They are preferred in the order they
    // were added, again whichever order the document uses.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alias("first_alias")
                            .alias("second_alias")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    for (std::string_view source : { R"({ "first_alias": 1, "second_alias": 50, "b": 2, "c": 3 })",
                                     R"({ "second_alias": 50, "first_alias": 1, "b": 2, "c": 3 })"
                                   })
    {
        deserialization_context cxt(fmt);
        auto               rdr = open(source);

        auto out = cxt.deserialize<triple>(rdr);
        ensure(out.has_value());
        ensure(cxt.problems().empty());
        ensure_eq(triple({ 1, 2, 3 }), *out);
    }
}

TEST(serialization_builder_an_alias_is_not_a_duplicate_key)
{
    // Naming one member two ways and repeating one key are different things, and only the second is
    // `duplicate_key_action`'s to refuse. Strict handling used to reject this document, which is valid.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alias("A")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    auto strict = deserialize_options::create_default()
                      .on_duplicate_key(deserialize_options::duplicate_key_action::exception);

    deserialization_context cxt(fmt, std::nullopt, jsonv::path(), nullptr, strict);
    auto               rdr = open(R"({ "a": 1, "A": 50, "b": 2, "c": 3 })");

    auto out = cxt.deserialize<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);

    // The same name twice still is one.
    deserialization_context repeated(fmt, std::nullopt, jsonv::path(), nullptr, strict);
    auto               repeated_rdr = open(R"({ "a": 1, "a": 50, "b": 2, "c": 3 })");

    ensure(!repeated.deserialize<triple>(repeated_rdr).has_value());
    ensure_eq(std::string("Duplicate key in object: \"a\""), repeated.problems().at(0).message());
}

TEST(serialization_builder_a_superseded_alias_is_not_checked)
{
    // The spelling which loses is stepped over unread, so a `check` on the member never sees it. Reading it
    // only to throw the result away would report failures against a value the type did not take.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alias("A")
                            .check([] (const std::int64_t& value)
                                   {
                                       if (value > 10)
                                           throw std::logic_error("a must be small");
                                   }
                                  )
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    deserialization_context cxt(fmt);
    auto               rdr = open(R"({ "a": 1, "A": 5000, "b": 2, "c": 3 })");

    auto out = cxt.deserialize<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);
}

TEST(serialization_builder_names_the_key_the_document_used)
{
    // A failure inside a member is reported at the key the *document* spelled, not at the name the member was
    // declared with -- which is what makes `alias` a matching rule rather than a rename.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alias("A")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    try
    {
        (void) deserialize<triple>(parse(R"({ "A": "not a number", "b": 2, "c": 3 })"), fmt);
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
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

    deserialization_context cxt(fmt);
    auto               rdr = open(R"({ "a\nb": 1, "b": 2, "c": 3 })");

    auto out = cxt.deserialize<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);
}

TEST(serialization_builder_names_an_escaped_key_it_failed_in)
{
    // The decoded key outlives the member's deserialization, so it is still there to name the failure.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a\nb", &triple::a)
                        .member("b",    &triple::b)
                        .member("c",    &triple::c)
                  .compose_checked(formats::defaults());

    deserialization_context cxt(fmt);
    auto               rdr = open(R"({ "a\nb": "not a number", "b": 2, "c": 3 })");

    ensure(!cxt.deserialize<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    // Spelled as an element rather than through `path::create`, which parses its argument and has no syntax for a
    // key containing a newline.
    ensure_eq(path({ "a\nb" }), cxt.problems().at(0).path());
}

TEST(serialization_builder_duplicate_key_replaces_by_default)
{
    // `deserialize_options::duplicate_key_action::replace` is the default and means the last spelling of a key wins,
    // which is what `parse_index::extract_tree` does with one.
    deserialization_context cxt(triple_formats());
    auto               rdr = open(R"({ "a": 1, "b": 2, "c": 3, "a": 99 })");

    auto out = cxt.deserialize<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 99, 2, 3 }), *out);
}

TEST(serialization_builder_duplicate_key_can_keep_the_first)
{
    deserialization_context cxt(triple_formats(),
                                std::nullopt,
                                jsonv::path(),
                                nullptr,
                                deserialize_options::create_default()
                                    .on_duplicate_key(deserialize_options::duplicate_key_action::ignore)
                               );
    auto rdr = open(R"({ "a": 1, "b": 2, "c": 3, "a": 99 })");

    auto out = cxt.deserialize<triple>(rdr);
    ensure(out.has_value());
    ensure(cxt.problems().empty());
    ensure_eq(triple({ 1, 2, 3 }), *out);
}

TEST(serialization_builder_duplicate_key_can_be_refused)
{
    deserialization_context cxt(triple_formats(),
                                std::nullopt,
                                jsonv::path(),
                                nullptr,
                                deserialize_options::create_default()
                                    .on_duplicate_key(deserialize_options::duplicate_key_action::exception)
                               );
    auto rdr = open(R"({ "a": 1, "b": 2, "c": 3, "a": 99 })");

    ensure(!cxt.deserialize<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Duplicate key in object: \"a\""), cxt.problems().at(0).message());
}

TEST(serialization_builder_refuses_a_non_object)
{
    // The walk asks for `{` and says what it found instead. This used to be a `kind_error` out of `value::find`,
    // because each member looked itself up in something which was not an object.
    deserialization_context cxt(triple_formats());
    auto               rdr = open("5");

    ensure(!cxt.deserialize<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure(cxt.problems().at(0).message().find("object") != std::string::npos);
}

TEST(serialization_builder_check_rejects_a_member)
{
    // `check` runs on the value which was read, before it reaches the member. It had never run at all: the
    // mutator it composes into was stored and never called.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .check([] (const std::int64_t& value)
                                   {
                                       if (value < 0)
                                           throw std::logic_error("a must not be negative");
                                   }
                                  )
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    ensure_eq(triple({ 1, 2, 3 }), deserialize<triple>(parse(R"({ "a": 1, "b": 2, "c": 3 })"), fmt));

    try
    {
        (void) deserialize<triple>(parse(R"({ "a": -1, "b": 2, "c": 3 })"), fmt);
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure(err.problems().at(0).nested_ptr());
        ensure_throws(std::logic_error, (std::rethrow_exception(err.problems().at(0).nested_ptr()), 0));
    }
}

TEST(serialization_builder_check_with_a_thrower_rejects_a_member)
{
    // The predicate says whether the value is acceptable and the thrower says how to refuse it, with the value in
    // hand. Neither two-argument form had ever compiled: a thrower lambda was an exact match for the exception form's
    // TException, which composed it with the predicate by calling itself again, and so on until the instantiation
    // depth ran out.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .check([] (const std::int64_t& value) { return value < 100; },
                                   [] (const std::int64_t& value)
                                   {
                                       throw std::out_of_range("a must be below 100, not " + std::to_string(value));
                                   }
                                  )
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    ensure_eq(triple({ 99, 2, 3 }), deserialize<triple>(parse(R"({ "a": 99, "b": 2, "c": 3 })"), fmt));

    try
    {
        (void) deserialize<triple>(parse(R"({ "a": 100, "b": 2, "c": 3 })"), fmt);
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
    {
        ensure(err.problems().at(0).nested_ptr());
        try
        {
            std::rethrow_exception(err.problems().at(0).nested_ptr());
        }
        catch (const std::out_of_range& ex)
        {
            ensure_eq(std::string("a must be below 100, not 100"), std::string(ex.what()));
        }
    }
}

TEST(serialization_builder_check_with_a_mutable_thrower_calls_it)
{
    // A thrower is anything callable with the value the way std::function calls its target: as a non-const lvalue.
    // A mutable lambda is not callable as a const one, and a constraint which asked that question sent every
    // mutable thrower to the exception form, which threw the lambda itself and never ran it.
    std::vector<std::int64_t> refused;
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .check([] (const std::int64_t& value) { return value < 100; },
                                   [&refused, calls = 0] (const std::int64_t& value) mutable
                                   {
                                       refused.push_back(value);
                                       throw std::out_of_range("refusal " + std::to_string(++calls));
                                   }
                                  )
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    for (int attempt : { 1, 2 })
    {
        try
        {
            (void) deserialize<triple>(parse(R"({ "a": 100, "b": 2, "c": 3 })"), fmt);
            ensure(!"deserialization_error was not thrown");
        }
        catch (const deserialization_error& err)
        {
            ensure(err.problems().at(0).nested_ptr());
            try
            {
                std::rethrow_exception(err.problems().at(0).nested_ptr());
            }
            catch (const std::out_of_range& ex)
            {
                if (attempt == 1)
                    ensure_eq(std::string("refusal 1"), std::string(ex.what()));
            }
        }
    }
    ensure(refused == std::vector<std::int64_t>({ 100, 100 }));
}

TEST(serialization_builder_check_with_an_exception_rejects_a_member)
{
    // The exception form takes anything which cannot be called with the value, which is what tells an exception from
    // a thrower.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .check([] (const std::int64_t& value) { return value % 2 == 0; },
                                   std::logic_error("a must be even")
                                  )
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    ensure_eq(triple({ 4, 2, 3 }), deserialize<triple>(parse(R"({ "a": 4, "b": 2, "c": 3 })"), fmt));

    try
    {
        (void) deserialize<triple>(parse(R"({ "a": 3, "b": 2, "c": 3 })"), fmt);
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
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

    wide out = deserialize<wide>(source, fmt);
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
        (void) deserialize<wide>(source, strict_builder.compose_checked(formats::defaults()));
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
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
                                             .type_default_value([] (deserialization_context&) -> triple
                                                                 {
                                                                     throw std::runtime_error("no default to give");
                                                                 }
                                                                )
                                         .register_container<std::vector<triple>>()
                                     .compose_checked(formats::defaults())
                                   });

    try
    {
        (void) deserialize<std::vector<triple>>(parse(R"([ null, { "a": "bad", "b": 2, "c": 3 } ])"),
                                                fmt,
                                                collecting()
                                               );
        ensure(!"deserialization_error was not thrown");
    }
    catch (const deserialization_error& err)
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
    // alias or a second of another. Strict handling therefore asks of the keys themselves, so the same object is
    // refused whichever order it listed them in.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .alias("A")
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    auto strict = deserialize_options::create_default()
                      .on_duplicate_key(deserialize_options::duplicate_key_action::exception);

    for (std::string_view source : { R"({ "A": 1, "a": 2, "A": 3, "b": 2, "c": 3 })",
                                     R"({ "A": 1, "A": 3, "a": 2, "b": 2, "c": 3 })"
                                   })
    {
        deserialization_context cxt(fmt, std::nullopt, jsonv::path(), nullptr, strict);
        auto               rdr = open(source);

        ensure(!cxt.deserialize<triple>(rdr).has_value());
        ensure_eq(1U, cxt.problems().size());
        ensure_eq(std::string("Duplicate key in object: \"A\""), cxt.problems().at(0).message());
    }
}

TEST(serialization_builder_strict_duplicates_cover_unrecognised_keys)
{
    // The policy is about the document repeating a key, which has nothing to do with whether this type happens to
    // want it -- and it is what `parse_index::extract_tree` refuses for the same document.
    auto strict = deserialize_options::create_default()
                      .on_duplicate_key(deserialize_options::duplicate_key_action::exception);

    deserialization_context cxt(triple_formats(), std::nullopt, jsonv::path(), nullptr, strict);
    auto               rdr = open(R"({ "a": 1, "b": 2, "c": 3, "extra": 1, "extra": 2 })");

    ensure(!cxt.deserialize<triple>(rdr).has_value());
    ensure_eq(1U, cxt.problems().size());
    ensure_eq(std::string("Duplicate key in object: \"extra\""), cxt.problems().at(0).message());
}

namespace
{

/// A \c triple inside another object, for telling which of the two a hook was shown.
struct wrapper
{
    triple       inner;
    std::int64_t d;
};

/// A \c triple which refuses a negative \c a once it is built, quoting the object it was read from.
formats refusing_triple_formats()
{
    static const formats instance =
        formats_builder()
            .type<triple>()
                .member("a", &triple::a)
                .member("b", &triple::b)
                .member("c", &triple::c)
                .post_deserialize([] (const deserialization_context& context, triple&& out)
                                  {
                                      if (out.a < 0)
                                          throw std::invalid_argument("Negative a in "
                                                                      + std::string(context.encoded_source())
                                                                     );

                                      return out;
                                  }
                                 )
        .compose_checked(formats::defaults());

    return instance;
}

/// The message of the problem deserializing \a source as a \c triple through \c refusing_triple_formats raised.
template <typename TSource>
std::string refusal_message(const TSource& source)
{
    try
    {
        (void) deserialize<triple>(source, refusing_triple_formats());
    }
    catch (const deserialization_error& err)
    {
        return err.problems().at(0).message();
    }

    ensure(!"deserialization_error was not thrown");
    return std::string();
}

}

TEST(serialization_builder_post_deserialize_quotes_the_text_it_read)
{
    // Read from text, the object is quoted as it was written: the spacing it had, and the key no member claimed still
    // in it, since what is quoted is the object's extent rather than what the walk kept of it. The whitespace around
    // the object is the document's rather than the object's, so it is left out.
    const std::string_view object = R"({"a":-1,  "extra" : [ 1, { "x": 2 } ],"b":2,"c":3   })";
    const std::string      text   = "\n  " + std::string(object) + "  \n";

    ensure_eq("Negative a in " + std::string(object), refusal_message(std::string_view(text)));
}

TEST(serialization_builder_post_deserialize_quotes_a_value_as_its_encoding)
{
    // A value has no text to view, so what is quoted is its encoding. That a reader over a value makes its `{` and `}`
    // out of one static `{}` is why the extent of the object cannot simply be read off the two of them, as it is from
    // text -- doing so would quote every object as `{}`.
    const value source = object({ { "a", -1 }, { "b", 2 }, { "c", 3 }, { "extra", array({ 1, 2 }) } });

    ensure_eq("Negative a in " + to_string(source), refusal_message(source));
}

TEST(serialization_builder_encoded_source_is_the_object_not_the_document)
{
    // The inner object's hook runs part-way through the walk of the outer one and is shown only its own object. The
    // outer one's runs after the inner has returned and is shown the whole outer object again.
    std::vector<std::string> seen;
    auto record = [&seen] (const deserialization_context& context) { seen.emplace_back(context.encoded_source()); };

    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                        .post_deserialize([record] (const deserialization_context& context, triple&& out)
                                          {
                                              record(context);
                                              return out;
                                          }
                                         )
                    .type<wrapper>()
                        .member("inner", &wrapper::inner)
                        .member("d",     &wrapper::d)
                        .post_deserialize([record] (const deserialization_context& context, wrapper&& out)
                                          {
                                              record(context);
                                              return out;
                                          }
                                         )
                  .compose_checked(formats::defaults());

    const std::string_view inner = R"({ "c": 3, "a": 1,   "b": 2 })";
    const std::string      outer = R"({ "d": 4, "inner": )" + std::string(inner) + R"(, "e": [ "unclaimed" ] })";

    (void) deserialize<wrapper>(std::string_view(outer), fmt);
    ensure_eq(2U, seen.size());
    ensure_eq(std::string(inner), seen.at(0));
    ensure_eq(outer, seen.at(1));

    seen.clear();
    const value tree = parse(outer);
    (void) deserialize<wrapper>(tree, fmt);
    ensure_eq(2U, seen.size());
    ensure_eq(to_string(tree.at("inner")), seen.at(0));
    ensure_eq(to_string(tree), seen.at(1));
}

TEST(serialization_builder_encoded_source_reaches_every_hook_after_the_walk)
{
    // The unknown-members handler and the default for a key which never arrived both run once the walk has reached the
    // `}`, as `post_deserialize` does, so they are shown the object as well.
    std::vector<std::string> seen;

    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                            .default_value([&seen] (const deserialization_context& context)
                                           {
                                               seen.emplace_back(context.encoded_source());
                                               return std::int64_t(3);
                                           }
                                          )
                        .on_unknown_members([&seen]
                                            (const deserialization_context& context, const std::set<std::string>&)
                                            {
                                                seen.emplace_back(context.encoded_source());
                                            }
                                           )
                  .compose_checked(formats::defaults());

    const std::string_view text = R"({ "a": 1, "b": 2, "extra": true })";

    ensure_eq(triple({ 1, 2, 3 }), deserialize<triple>(text, fmt));
    ensure_eq(2U, seen.size());
    ensure_eq(std::string(text), seen.at(0));
    ensure_eq(std::string(text), seen.at(1));
}

TEST(serialization_builder_encoded_source_is_empty_until_the_object_closes)
{
    // Until the walk reaches the `}` there is no whole object to quote, whichever source it is read from: not in
    // `pre_deserialize`, and not in a member's `check` -- including one inside a nested object, whose enclosing
    // object is no more finished than it is.
    const deserialization_context* current = nullptr;
    std::vector<std::string>  seen;
    auto record = [&current, &seen] { seen.emplace_back(current->encoded_source()); };

    formats fmt = formats_builder()
                    .type<triple>()
                        .pre_deserialize([record] (const deserialization_context&) { record(); })
                        .member("a", &triple::a)
                            .check([record] (const std::int64_t&) { record(); })
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                    .type<wrapper>()
                        .pre_deserialize([record] (const deserialization_context&) { record(); })
                        .member("inner", &wrapper::inner)
                        .member("d",     &wrapper::d)
                            .check([record] (const std::int64_t&) { record(); })
                  .compose_checked(formats::defaults());

    const std::string_view text = R"({ "inner": { "a": 1, "b": 2, "c": 3 }, "d": 4 })";

    for (bool from_text : { true, false })
    {
        deserialization_context cxt(fmt);
        current = &cxt;
        seen.clear();

        if (from_text)
        {
            auto rdr = open(text);
            ensure(cxt.deserialize<wrapper>(rdr).has_value());
        }
        else
        {
            (void) cxt.deserialize<wrapper>(parse(text));
        }

        // Both `pre_deserialize`s, then the inner `check` and then the outer one.
        ensure_eq(4U, seen.size());
        for (const auto& source : seen)
            ensure_eq(std::string(), source);

        ensure(cxt.encoded_source().empty());
    }

    ensure(deserialization_context().encoded_source().empty());
}

TEST(serialization_builder_encoded_source_hides_an_enclosing_object)
{
    // A hook is free to deserialize something else. That deserialization's hooks are shown their own object or nothing,
    // never the object whose hook started it -- and once it has returned, that object is shown again.
    std::vector<std::string> seen;

    formats fmt = formats_builder()
                    .type<triple>()
                        .pre_deserialize([&seen] (const deserialization_context& context)
                                         {
                                             seen.emplace_back(context.encoded_source());
                                         }
                                        )
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                    .type<wrapper>()
                        .member("d", &wrapper::d)
                        .post_deserialize([&seen] (deserialization_context& context, wrapper&& out)
                                          {
                                              seen.emplace_back(context.encoded_source());

                                              auto rdr = open(R"({ "a": 1, "b": 2, "c": 3 })");
                                              out.inner = context.deserialize<triple>(rdr).value();

                                              seen.emplace_back(context.encoded_source());
                                              return out;
                                          }
                                         )
                  .compose_checked(formats::defaults());

    const std::string_view text = R"({ "d": 4 })";

    ensure_eq(triple({ 1, 2, 3 }), deserialize<wrapper>(text, fmt).inner);
    ensure_eq(3U, seen.size());
    ensure_eq(std::string(text), seen.at(0));
    ensure_eq(std::string(),     seen.at(1));
    ensure_eq(std::string(text), seen.at(2));
}

namespace
{

/// Deserialized by a hand-written deserializer rather than the DSL, keeping what \c encoded_source showed it.
struct source_probe
{
    std::string seen;
};

}

TEST(serialization_builder_encoded_source_hides_an_enclosing_object_from_any_deserializer)
{
    // The DSL's adapter is not the only deserializer a hook might start, and one written by hand makes no scope of its
    // own to hide the enclosing object behind. It is shown nothing all the same, whether it is reached through a reader
    // or through a `value` -- and once it has returned, the hook's own object is shown again.
    static auto probe = make_deserializer([] (deserialization_context& context, reader& from) -> source_probe
                                          {
                                              (void) from.next_value();
                                              return source_probe{ std::string(context.encoded_source()) };
                                          });

    std::vector<std::string> seen;

    formats dsl = formats_builder()
                    .type<wrapper>()
                        .member("d", &wrapper::d)
                        .post_deserialize([&seen] (deserialization_context& context, wrapper&& out)
                                          {
                                              seen.emplace_back(context.encoded_source());

                                              auto rdr = open("42");
                                              seen.push_back(context.deserialize<source_probe>(rdr)->seen);
                                              seen.push_back(context.deserialize<source_probe>(value(42)).seen);

                                              seen.emplace_back(context.encoded_source());
                                              return out;
                                          }
                                         )
                  .compose_checked(formats::defaults());

    formats fmt = formats::compose({ dsl });
    fmt.register_deserializer(&probe);

    const std::string_view text = R"({ "d": 4 })";
    const value            tree = parse(text);

    for (bool from_text : { true, false })
    {
        seen.clear();
        if (from_text)
            (void) deserialize<wrapper>(text, fmt);
        else
            (void) deserialize<wrapper>(tree, fmt);

        const std::string outer = from_text ? std::string(text) : to_string(tree);
        ensure_eq(4U, seen.size());
        ensure_eq(outer,         seen.at(0));
        ensure_eq(std::string(), seen.at(1));
        ensure_eq(std::string(), seen.at(2));
        ensure_eq(outer,         seen.at(3));
    }
}

TEST(serialization_builder_encoded_source_reaches_every_default)
{
    // A default is taken after the walk, factory and setter both, and both can quote the object -- whether the key
    // never arrived or held a `null` standing for the default under `default_on_null`, which means the same thing.
    // The setter of a member whose key held a value runs as the walk meets it, before there is a `}`, and is shown
    // nothing.
    const deserialization_context* current = nullptr;
    std::vector<std::string>  seen;

    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member<std::int64_t>("c",
                                              [] (const triple& x) -> const std::int64_t& { return x.c; },
                                              [&current, &seen] (triple& x, std::int64_t&& c)
                                              {
                                                  seen.emplace_back(current->encoded_source());
                                                  x.c = c;
                                              }
                                             )
                            .default_value([&seen] (const deserialization_context& context)
                                           {
                                               seen.emplace_back(context.encoded_source());
                                               return std::int64_t(3);
                                           }
                                          )
                            .default_on_null()
                  .compose_checked(formats::defaults());

    auto run = [&] (std::string_view text, bool from_text)
               {
                   deserialization_context cxt(fmt);
                   current = &cxt;
                   seen.clear();

                   if (from_text)
                   {
                       auto rdr = open(text);
                       ensure(cxt.deserialize<triple>(rdr).has_value());
                   }
                   else
                   {
                       (void) cxt.deserialize<triple>(parse(text));
                   }

                   return seen;
               };

    const std::string_view missing = R"({ "a": 1, "b": 2 })";
    const std::string_view nulled  = R"({ "a": 1, "b": 2, "c": null })";

    for (bool from_text : { true, false })
    {
        auto quote = [from_text] (std::string_view text)
                     {
                         return from_text ? std::string(text) : to_string(parse(text));
                     };

        // The factory, then the setter it hands its value to.
        ensure(run(missing, from_text) == std::vector<std::string>({ quote(missing), quote(missing) }));
        ensure(run(nulled, from_text) == std::vector<std::string>({ quote(nulled), quote(nulled) }));
        ensure(run(R"({ "a": 1, "b": 2, "c": 3 })", from_text) == std::vector<std::string>({ "" }));
    }
}

TEST(serialization_builder_encoded_source_encodes_a_value_once)
{
    // Every hook of one object is shown the same encoding, rather than each paying to build its own. The second hook
    // compares while the first one's view is still valid, which it is until the adapter returns.
    const char* first = nullptr;
    bool        same  = false;

    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                        .post_deserialize([&first] (const deserialization_context& context, triple&& out)
                                          {
                                              first = context.encoded_source().data();
                                              return out;
                                          }
                                         )
                        .post_deserialize([&first, &same] (const deserialization_context& context, triple&& out)
                                          {
                                              same = context.encoded_source().data() == first;
                                              return out;
                                          }
                                         )
                  .compose_checked(formats::defaults());

    (void) deserialize<triple>(object({ { "a", 1 }, { "b", 2 }, { "c", 3 } }), fmt);
    ensure(first != nullptr);
    ensure(same);
}


TEST(serialization_builder_default_value_reads_its_siblings)
{
    // A default is taken once the walk is done, so it can read any member of the object: one the document lists after
    // the key the default stands in for as well as one before it, and whether that key never arrived or held a `null`.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                            .default_value([] (deserialization_context& context)
                                           {
                                               const value& source = context.source_value().value();
                                               return source.at("a").as_integer() + source.at("b").as_integer();
                                           }
                                          )
                            .default_on_null()
                  .compose_checked(formats::defaults());

    for (std::string_view text : { R"({ "a": 1, "b": 2 })",
                                   R"({ "c": null, "a": 1, "b": 2 })",
                                   R"({ "a": 1, "c": null, "b": 2 })",
                                 }
        )
    {
        ensure_eq(triple({ 1, 2, 3 }), deserialize<triple>(text, fmt));
        ensure_eq(triple({ 1, 2, 3 }), deserialize<triple>(parse(text), fmt));
    }

    // A value for the key is deserialized rather than defaulted, wherever a `null` for it was.
    ensure_eq(triple({ 1, 2, 7 }),
              deserialize<triple>(std::string_view(R"({ "c": null, "a": 1, "b": 2, "c": 7 })"), fmt)
             );
}

TEST(serialization_builder_null_default_waits_for_the_walk)
{
    // A `null` under `default_on_null` means what a missing key means, so its default is taken where a missing key's
    // is: after every key the document has, in declaration order, rather than as the walk meets the `null`.
    std::vector<std::string> order;

    auto setter = [&order] (const std::string& name)
                  {
                      return [&order, name] (triple& x, std::int64_t&& v)
                             {
                                 order.push_back(name);
                                 if (name == "a")
                                     x.a = v;
                                 else if (name == "b")
                                     x.b = v;
                                 else
                                     x.c = v;
                             };
                  };

    formats fmt = formats_builder()
                    .type<triple>()
                        .member<std::int64_t>("a",
                                              [] (const triple& x) -> const std::int64_t& { return x.a; },
                                              setter("a")
                                             )
                            .default_value(std::int64_t(10))
                            .default_on_null()
                        .member<std::int64_t>("b",
                                              [] (const triple& x) -> const std::int64_t& { return x.b; },
                                              setter("b")
                                             )
                        .member<std::int64_t>("c",
                                              [] (const triple& x) -> const std::int64_t& { return x.c; },
                                              setter("c")
                                             )
                            .default_value(std::int64_t(30))
                            .default_on_null()
                  .compose_checked(formats::defaults());

    for (bool from_text : { true, false })
    {
        const std::string_view text = R"({ "c": null, "b": 2, "a": null })";

        order.clear();
        auto out = from_text ? deserialize<triple>(text, fmt) : deserialize<triple>(parse(text), fmt);
        ensure_eq(triple({ 10, 2, 30 }), out);
        ensure(order == std::vector<std::string>({ "b", "a", "c" }));
    }
}

namespace
{

/// A single member answering to \c a0 and to \c a1 through \c a32767 after it.
struct many_named
{
    std::int64_t a;
};

/// More members than the DSL's walk tracks inline: \c m0 through \c m69, each defaulting to its own index.
struct wide
{
    std::array<std::int64_t, 70U> values;
};

}

TEST(serialization_builder_null_default_keeps_its_claim_with_many_names)
{
    // A `null` taking the default is still a claim by the name it came under, however far down the member's list of
    // names that is: a repeat of the name is settled against it as against any claim. The way the walk remembers that
    // the claim was a `null` must not make the claim look like none at all.
    for (auto action : { deserialize_options::duplicate_key_action::ignore,
                         deserialize_options::duplicate_key_action::replace })
    {
        auto builder = formats_builder();
        auto member  = builder.type<many_named>().member("a0", &many_named::a);
        for (int idx = 1; idx < 32768; ++idx)
            member.alias("a" + std::to_string(idx));
        member.default_value(std::int64_t(7)).default_on_null();

        formats fmt = builder.compose_checked(formats::defaults());

        // Only text can say a key twice; a `value` has already settled which of the two it keeps.
        const std::string_view text     = R"({ "a32767": null, "a32767": 9 })";
        const auto             options  = deserialize_options().on_duplicate_key(action);
        const std::int64_t     expected = action == deserialize_options::duplicate_key_action::ignore ? 7 : 9;

        ensure_eq(expected, deserialize<many_named>(text, fmt, options).a);
        ensure_eq(7,        deserialize<many_named>(parse(R"({ "a32767": null })"), fmt, options).a);
    }
}

TEST(serialization_builder_null_default_on_a_type_too_wide_to_track_inline)
{
    auto builder = formats_builder();
    auto type    = builder.type<wide>();
    for (std::size_t idx = 0U; idx < 70U; ++idx)
    {
        type.member<std::int64_t>("m" + std::to_string(idx),
                                  [idx] (const wide& x) -> const std::int64_t& { return x.values[idx]; },
                                  [idx] (wide& x, std::int64_t&& v) { x.values[idx] = v; }
                                 )
                .default_value(std::int64_t(idx))
                .default_on_null();
    }
    formats fmt = builder.compose_checked(formats::defaults());

    // Members on either side of the inline limit, defaulted by a `null` and by having no key, and one with a value.
    const std::string_view text = R"({ "m3": null, "m65": null, "m66": 500, "m69": null })";

    for (bool from_text : { true, false })
    {
        auto out = from_text ? deserialize<wide>(text, fmt) : deserialize<wide>(parse(text), fmt);
        for (std::size_t idx = 0U; idx < 70U; ++idx)
            ensure_eq(idx == 66U ? 500 : std::int64_t(idx), out.values[idx]);
    }
}

TEST(serialization_builder_unknown_members_handler_reads_their_values)
{
    // The handler is handed the names of the keys no member claimed, and can read their values out of the object.
    value seen;

    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                        .on_unknown_members([&seen]
                                            (deserialization_context& context, const std::set<std::string>& keys)
                                            {
                                                const value& source = context.source_value().value();
                                                for (const auto& key : keys)
                                                    seen[key] = source.at(key);
                                            }
                                           )
                  .compose_checked(formats::defaults());

    const std::string_view text = R"({ "a": 1, "x": [ 1, { "y": true } ], "b": 2, "c": 3, "z": "zed" })";
    const value            expected = object({ { "x", array({ 1, object({ { "y", true } }) }) }, { "z", "zed" } });

    for (bool from_text : { true, false })
    {
        seen = object();
        auto out = from_text ? deserialize<triple>(text, fmt) : deserialize<triple>(parse(text), fmt);
        ensure_eq(triple({ 1, 2, 3 }), out);
        ensure_eq(expected, seen);
    }
}

TEST(serialization_builder_pre_deserialize_reads_the_document)
{
    // Run before the walk, `pre_deserialize` can refuse a document by what it says before any member has been read.
    formats fmt = formats_builder()
                    .type<triple>()
                        .pre_deserialize([] (deserialization_context& context)
                                         {
                                             const value& source = context.source_value().value();
                                             if (source.count("schema") == 0U || source.at("schema") != value(2))
                                                 throw std::invalid_argument("Unsupported schema");
                                         }
                                        )
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    const std::string_view current  = R"({ "schema": 2, "a": 1, "b": 2, "c": 3 })";
    const std::string_view outdated = R"({ "schema": 1, "a": 1, "b": 2, "c": 3 })";

    ensure_eq(triple({ 1, 2, 3 }), deserialize<triple>(current, fmt));
    ensure_eq(triple({ 1, 2, 3 }), deserialize<triple>(parse(current), fmt));
    ensure_throws(deserialization_error, deserialize<triple>(outdated, fmt));
    ensure_throws(deserialization_error, deserialize<triple>(parse(outdated), fmt));
}

namespace
{

/// What \c deserialization_context::source_value showed, as text: the encoding of the value, or "nothing".
std::string shown(const deserialization_context& context)
{
    auto source = context.source_value();
    return source ? to_string(*source) : std::string("nothing");
}

}

TEST(serialization_builder_pre_deserialize_is_shown_what_the_reader_is_on)
{
    // Before the walk there is no knowing that the value is an object, and `pre_deserialize` is shown it whatever it
    // is. A `type_default_value` standing in for a `null` is shown nothing, as the cursor has stepped past what it
    // replaces.
    std::vector<std::string> seen;

    formats fmt = formats_builder()
                    .type<triple>()
                        .pre_deserialize([&seen] (deserialization_context& context) { seen.push_back(shown(context)); })
                        .type_default_on_null()
                        .type_default_value([&seen] (deserialization_context& context)
                                            {
                                                seen.push_back(shown(context));
                                                return triple({ 0, 0, 0 });
                                            }
                                           )
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                  .compose_checked(formats::defaults());

    for (bool from_text : { true, false })
    {
        seen.clear();
        auto out = from_text ? deserialize<triple>(std::string_view("null"), fmt) : deserialize<triple>(null, fmt);
        ensure_eq(triple({ 0, 0, 0 }), out);
        ensure(seen == std::vector<std::string>({ "null", "nothing" }));

        seen.clear();
        ensure_throws(deserialization_error,
                      from_text ? deserialize<triple>(std::string_view("[ 1 ]"), fmt)
                                : deserialize<triple>(array({ 1 }), fmt)
                     );
        ensure(seen == std::vector<std::string>({ to_string(array({ 1 })) }));
    }
}

TEST(serialization_builder_source_value_is_empty_during_the_walk)
{
    // A member's `check` and setter run as the walk meets their key, and are shown nothing -- including in a
    // nested object, whose enclosing object is part-way through its own walk.
    const deserialization_context* current = nullptr;
    std::vector<std::string>  seen;
    auto record = [&current, &seen] { seen.push_back(shown(*current)); };

    formats fmt = formats_builder()
                    .type<triple>()
                        .member("a", &triple::a)
                            .check([record] (const std::int64_t&) { record(); })
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                    .type<wrapper>()
                        .member("inner", &wrapper::inner)
                        .member("d",     &wrapper::d)
                            .check([record] (const std::int64_t&) { record(); })
                  .compose_checked(formats::defaults());

    const std::string_view text = R"({ "inner": { "a": 1, "b": 2, "c": 3 }, "d": 4 })";

    for (bool from_text : { true, false })
    {
        deserialization_context cxt(fmt);
        current = &cxt;
        seen.clear();

        if (from_text)
        {
            auto rdr = open(text);
            ensure(cxt.deserialize<wrapper>(rdr).has_value());
        }
        else
        {
            (void) cxt.deserialize<wrapper>(parse(text));
        }

        ensure(seen == std::vector<std::string>({ "nothing", "nothing" }));
        ensure(!cxt.source_value());
    }

    ensure(!deserialization_context().source_value());
}

namespace
{

/// Deserialized by a hand-written deserializer rather than the DSL, keeping what \c source_value showed it.
struct source_value_probe
{
    std::string seen;
};

}

TEST(serialization_builder_source_value_hides_an_enclosing_object)
{
    // A hook is free to deserialize something else. What that deserialization runs is shown its own object or nothing,
    // never the object whose hook started it -- whether it is the DSL's or a deserializer written by hand -- and once
    // it has returned, that object is shown again.
    static auto probe = make_deserializer([] (deserialization_context& context, reader& from) -> source_value_probe
                                          {
                                              (void) from.next_value();
                                              return source_value_probe{ shown(context) };
                                          });

    std::vector<std::string> seen;

    formats dsl = formats_builder()
                    .type<triple>()
                        .pre_deserialize([&seen] (deserialization_context& context) { seen.push_back(shown(context)); })
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                    .type<wrapper>()
                        .pre_deserialize([&seen] (deserialization_context& context)
                                         {
                                             seen.push_back(shown(context));

                                             auto rdr = open("42");
                                             seen.push_back(context.deserialize<source_value_probe>(rdr)->seen);
                                         }
                                        )
                        .member("d", &wrapper::d)
                        .post_deserialize([&seen] (deserialization_context& context, wrapper&& out)
                                          {
                                              seen.push_back(shown(context));

                                              auto rdr = open(R"({ "c": 3, "b": 2, "a": 1 })");
                                              out.inner = context.deserialize<triple>(rdr).value();
                                              seen.push_back(context.deserialize<source_value_probe>(value(42)).seen);

                                              seen.push_back(shown(context));
                                              return out;
                                          }
                                         )
                  .compose_checked(formats::defaults());

    formats fmt = formats::compose({ dsl });
    fmt.register_deserializer(&probe);

    const std::string_view text = R"({ "d": 4 })";

    for (bool from_text : { true, false })
    {
        seen.clear();
        auto out = from_text ? deserialize<wrapper>(text, fmt) : deserialize<wrapper>(parse(text), fmt);
        ensure_eq(triple({ 1, 2, 3 }), out.inner);

        const std::string outer = to_string(parse(text));
        const std::string inner = to_string(parse(R"({ "c": 3, "b": 2, "a": 1 })"));
        ensure(seen == std::vector<std::string>({ outer, "nothing", outer, inner, "nothing", outer }));
    }
}

TEST(serialization_builder_source_value_is_read_from_text_once)
{
    // From text, the first hook to ask has the object read, and every hook after it -- before the walk or after -- is
    // shown the same one. From a value, every hook is shown the caller's own tree.
    std::vector<const value*> seen;
    auto record = [&seen] (const deserialization_context& context) { seen.push_back(&context.source_value().value()); };

    formats fmt = formats_builder()
                    .type<triple>()
                        .pre_deserialize([record] (deserialization_context& context) { record(context); })
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                        .member("c", &triple::c)
                            .default_value([record] (deserialization_context& context)
                                           {
                                               record(context);
                                               return std::int64_t(3);
                                           }
                                          )
                        .post_deserialize([record] (deserialization_context& context, triple&& out)
                                          {
                                              record(context);
                                              return out;
                                          }
                                         )
                  .compose_checked(formats::defaults());

    const std::string_view text = R"({ "a": 1, "b": 2 })";

    (void) deserialize<triple>(text, fmt);
    ensure_eq(3U, seen.size());
    ensure(seen.at(0) == seen.at(1));
    ensure(seen.at(0) == seen.at(2));

    seen.clear();
    const value tree = parse(text);
    (void) deserialize<triple>(tree, fmt);
    ensure(seen == std::vector<const value*>({ &tree, &tree, &tree }));
}

namespace
{

/// A name and a nickname, both views of whatever they were deserialized from. The nickname defaults to the name.
struct nicknamed
{
    std::string_view name;
    std::string_view nick;
};

formats nicknamed_formats()
{
    static const formats instance =
        formats_builder()
            .type<nicknamed>()
                .pre_deserialize([] (deserialization_context& context) { (void) context.source_value(); })
                .member("name", &nicknamed::name)
                .member("nick", &nicknamed::nick)
                    .default_value([] (deserialization_context& context)
                                   {
                                       const value& source = context.source_value().value();
                                       return context.deserialize<std::string_view>(source.at("name"));
                                   }
                                  )
        .compose_checked(formats::defaults());

    return instance;
}

}

TEST(serialization_builder_source_value_read_from_text_is_temporary)
{
    // What is read out of text for a hook dies with the deserialization, so a view of it would dangle: deserializing
    // one is refused. A member read during the walk still views the text, even though `pre_deserialize` had the object
    // read.
    const std::string text = R"({ "name": "Robert", "nick": "Bob" })";

    auto out = deserialize<nicknamed>(std::string_view(text), nicknamed_formats());
    ensure_eq("Robert", out.name);
    ensure_eq("Bob",    out.nick);
    ensure(out.name.data() == text.data() + text.find("Robert"));

    ensure_throws(deserialization_error,
                  deserialize<nicknamed>(std::string_view(R"({ "name": "Robert" })"), nicknamed_formats())
                 );
}

TEST(serialization_builder_source_value_lent_from_a_value_may_be_viewed)
{
    // From a value, the hook is lent the caller's own tree, which a view may name.
    const value tree = object({ { "name", "Robert" } });

    auto out = deserialize<nicknamed>(tree, nicknamed_formats());
    ensure_eq("Robert", out.nick);
    ensure(out.nick.data() == tree.at("name").as_string_view().data());
}

TEST(serialization_builder_writes_members_in_declaration_order)
{
    // The members used to be collected into a `value`, whose object keeps its keys sorted. Written straight into the
    // writer, they come out in the order they were declared. The `value` `to_json` builds still sorts them, because
    // every `value` does.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("c", &triple::c)
                        .member("a", &triple::a)
                        .member("b", &triple::b)
                  .compose_checked(formats::defaults());

    const triple x{ 1, 2, 3 };
    ensure_eq(std::string(R"({"c":3,"a":1,"b":2})"), serialize_to_text(fmt, x));
    ensure_eq(std::string(R"({"a":1,"b":2,"c":3})"), to_string(to_json(x, fmt)));
}

TEST(serialization_builder_version_checks_skip_the_key_on_the_text_path)
{
    // `since` is covered above; `until`, `after` and `before` compose on `serialize_if` the same way, and a member any
    // of them leaves out is left out key and all.
    formats fmt = formats_builder()
                    .type<triple>()
                        .member("c", &triple::c)
                            .until({ 3, 0 })
                        .member("a", &triple::a)
                            .after({ 1, 0 })
                        .member("b", &triple::b)
                            .before({ 4, 0 })
                  .compose_checked(formats::defaults());

    const triple x{ 1, 2, 3 };
    auto text_ver = [&] (std::optional<version> v)
                    {
                        return serialize_to_text(serialization_context(fmt, v), x);
                    };

    ensure_eq(std::string(R"({"c":3,"a":1,"b":2})"), text_ver(std::nullopt));
    ensure_eq(std::string(R"({"c":3,"b":2})"),       text_ver(version(1, 0)));
    ensure_eq(std::string(R"({"c":3,"a":1,"b":2})"), text_ver(version(3, 0)));
    ensure_eq(std::string(R"({"a":1,"b":2})"),       text_ver(version(3, 5)));
    ensure_eq(std::string(R"({"a":1})"),             text_ver(version(4, 0)));
}

TEST(serialization_builder_refuses_a_duplicate_member_name)
{
    // Two members under one name used to be collected into a `value`, where `insert` kept the first and dropped the
    // second without a word. It is a mistake in the description, so it is refused where it is made.
    try
    {
        formats_builder()
            .type<triple>()
                .member("a", &triple::a)
                .member("a", &triple::b);
        ensure(!"std::invalid_argument was not thrown");
    }
    catch (const std::invalid_argument& ex)
    {
        ensure(std::string(ex.what()).find("\"a\"") != std::string::npos);
    }

    // Every `member` overload reaches the same check.
#ifndef _MSC_VER
    ensure_throws(std::invalid_argument,
                  formats_builder()
                      .type<wrapped_things>()
                          .member("x", &wrapped_things::x, &wrapped_things::x)
                          .member("x", &wrapped_things::y, &wrapped_things::y)
                 );
#endif
}

namespace
{

/// Nothing registers a serializer for this.
struct unserializable
{ };

struct inner_record
{
    unserializable x;
};

struct outer_record
{
    inner_record i;
};

}

TEST(serialization_builder_names_the_member_a_serialization_error_is_at)
{
    // Each member used to be serialized through `to_json`, in a writer of its own, so a failure inside one reported
    // a path from that member's root. Written into the caller's writer, the path is the member's: the `x` of the `i`
    // of the record. `compose_checked` would refuse this `formats` for the missing serializer, which is the point.
    formats fmt = formats::compose({ formats_builder()
                                         .type<outer_record>()
                                             .member("i", &outer_record::i)
                                         .type<inner_record>()
                                             .member("x", &inner_record::x),
                                     formats::defaults()
                                   });

    auto path_of_failure = [] (auto&& serialize_outer) -> path
                           {
                               try
                               {
                                   serialize_outer();
                                   ensure(!"no_serializer was not thrown");
                                   return path();
                               }
                               catch (const no_serializer& ex)
                               {
                                   ensure(ex.type_index() == std::type_index(typeid(unserializable)));
                                   return ex.path();
                               }
                           };

    serialization_context context(fmt);
    std::ostringstream    os;
    writer                to(os);
    ensure_eq(path::create(".i.x"), path_of_failure([&] { context.serialize(outer_record{}, to); }));
    ensure_eq(path::create(".i.x"), path_of_failure([&] { (void) to_json(outer_record{}, fmt); }));

    // What was written before the failure is left in the caller's writer, as `serialize` says it is.
    ensure_eq(std::string(R"({"i":{"x":)"), std::move(os).str());
}

namespace
{

/// A subtype of \c base which declares its members out of sorted order, so that text written straight into a writer
/// can be told from text written from a \c value, which sorts them.
struct d_derived :
        base
{
    virtual std::string get() const override { return "d"; }

    static void json_adapt(adapter_builder<d_derived>& builder)
    {
        builder.member("type", &d_derived::type);
        builder.member("z", &d_derived::z);
        builder.member("a", &d_derived::a);
    }

    std::string  type = "d";
    std::int64_t z    = 26;
    std::int64_t a    = 1;
};

/// \c d_derived without the discriminator, for \c keyed_subtype_action::insert to add.
struct e_derived :
        base
{
    virtual std::string get() const override { return "e"; }

    static void json_adapt(adapter_builder<e_derived>& builder)
    {
        builder.member("z", &e_derived::z);
        builder.member("a", &e_derived::a);
    }

    std::int64_t z = 26;
    std::int64_t a = 1;
};

/// A subtype with a member nothing can serialize.
struct f_derived :
        base
{
    virtual std::string get() const override { return "f"; }

    std::string    type = "f";
    unserializable u;
};

using base_list = std::vector<std::unique_ptr<base>>;

/// Formats for a \c base_list keyed by `"type"`, with \c d_derived registered under \a d_action and \c e_derived under
/// \c keyed_subtype_action::insert.
formats keyed_bases(keyed_subtype_action d_action)
{
    return formats_builder()
               .polymorphic_type<std::unique_ptr<base>>("type")
                   .check_null_output()
                   .subtype<a_derived>("a")
                   .subtype<d_derived>("d", d_action)
                   .subtype<e_derived>("e", keyed_subtype_action::insert)
               .type<a_derived>(a_derived::json_adapt)
               .type<d_derived>(d_derived::json_adapt)
               .type<e_derived>(e_derived::json_adapt)
               .register_container<base_list>()
           .compose_checked(formats::defaults());
}

}

TEST(serialization_builder_polymorphic_writes_a_subtype_where_the_writer_is)
{
    // A subtype used to be built as a `value` through `to_json` and written whole, which sorted its members. With no
    // action to run on the finished object, it is written straight into the writer, in the order it declares them.
    base_list from;
    from.push_back(std::make_unique<a_derived>());
    from.push_back(std::make_unique<d_derived>());
    from.push_back(nullptr);

    const formats keyed = keyed_bases(keyed_subtype_action::none);
    ensure_eq(std::string(R"([{"type":"a"},{"type":"d","z":26,"a":1},null])"), serialize_to_text(keyed, from));
    ensure_eq(std::string(R"([{"type":"a"},{"a":1,"type":"d","z":26},null])"), to_string(to_json(from, keyed)));

    // A subtype a general discriminator picks has no action either.
    const formats by_predicate = formats_builder()
                                     .polymorphic_type<std::unique_ptr<base>>()
                                         .subtype<d_derived>([] (const value& v)
                                                             {
                                                                 return v.is_object() && v.count("z") != 0U;
                                                             }
                                                            )
                                     .type<d_derived>(d_derived::json_adapt)
                                 .compose_checked(formats::defaults());
    ensure_eq(std::string(R"({"type":"d","z":26,"a":1})"),
              serialize_to_text(by_predicate, std::unique_ptr<base>(std::make_unique<d_derived>()))
             );
}

TEST(serialization_builder_polymorphic_keyed_actions_write_what_to_json_builds)
{
    // `check` and `insert` work on the finished object, so a subtype under either is still built as a `value` and
    // written whole: its text is what `to_json` builds, with the members sorted.
    base_list from;
    from.push_back(std::make_unique<d_derived>());
    from.push_back(std::make_unique<e_derived>());

    const formats     fmt  = keyed_bases(keyed_subtype_action::check);
    const std::string text = serialize_to_text(fmt, from);
    ensure_eq(to_string(to_json(from, fmt)), text);
    ensure_eq(std::string(R"([{"a":1,"type":"d","z":26},{"a":1,"type":"e","z":26}])"), text);
}

TEST(serialization_builder_polymorphic_refused_keyed_subtype_writes_nothing)
{
    // The action runs on the subtype's `value` before any of it is written, so a subtype it refuses leaves the writer
    // just past the element before it, and the refusal is placed at the slot the subtype was going into.
    auto refusal = [] (const formats& fmt, const base_list& from) -> std::string
                   {
                       std::ostringstream os;
                       writer             to(os);
                       try
                       {
                           serialization_context(fmt).serialize(from, to);
                           ensure(!"serialization_error was not thrown");
                       }
                       catch (const serialization_error& ex)
                       {
                           ensure_eq(path::create("[1]"), ex.path());
                           ensure(ex.type_index() == std::type_index(typeid(std::unique_ptr<base>)));
                           ensure(ex.nested_ptr() != nullptr);
                           ensure_eq(std::string(R"([{"type":"a"})"), os.str());
                           return ex.message();
                       }
                       return std::string();
                   };

    // `d_derived` writes its own `"type"`, which `insert` refuses...
    base_list writes_its_own;
    writes_its_own.push_back(std::make_unique<a_derived>());
    writes_its_own.push_back(std::make_unique<d_derived>());
    ensure(refusal(keyed_bases(keyed_subtype_action::insert), writes_its_own)
               .starts_with("Subtype key already present when trying to insert.")
          );

    // ...and `check` refuses one which says it is something else.
    base_list mislabelled;
    mislabelled.push_back(std::make_unique<a_derived>());
    mislabelled.push_back(std::make_unique<d_derived>());
    static_cast<d_derived&>(*mislabelled.back()).type = "e";
    ensure(refusal(keyed_bases(keyed_subtype_action::check), mislabelled)
               .starts_with("Expected subtype key is not the expected value.")
          );
}

TEST(serialization_builder_polymorphic_subtype_failure_is_placed_in_the_document)
{
    // A subtype used to be serialized through `to_json`, in a writer of its own, so a failure inside it reported a path
    // from the subtype's root. Written into the caller's writer, the path is the member's place in the document.
    // `compose_checked` would refuse this `formats` for the missing serializer, which is the point.
    formats fmt = formats::compose({ formats_builder()
                                         .polymorphic_type<std::unique_ptr<base>>("type")
                                             .subtype<a_derived>("a")
                                             .subtype<f_derived>("f")
                                         .type<a_derived>(a_derived::json_adapt)
                                         .type<f_derived>()
                                             .member("type", &f_derived::type)
                                             .member("u", &f_derived::u)
                                         .register_container<base_list>(),
                                     formats::defaults()
                                   });

    base_list from;
    from.push_back(std::make_unique<a_derived>());
    from.push_back(std::make_unique<f_derived>());

    std::ostringstream os;
    writer             to(os);
    try
    {
        serialization_context(fmt).serialize(from, to);
        ensure(!"no_serializer was not thrown");
    }
    catch (const no_serializer& ex)
    {
        ensure_eq(path::create("[1].u"), ex.path());
        ensure(ex.type_index() == std::type_index(typeid(unserializable)));
    }
    ensure_eq(std::string(R"([{"type":"a"},{"type":"f","u":)"), os.str());
}

}
