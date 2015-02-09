/** \file
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
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
           std::set<long>    favorite_numbers = {},
           std::vector<long> winning_numbers  = {}
          ) :
            firstname(std::move(f)),
            lastname(std::move(l)),
            age(a),
            favorite_numbers(std::move(favorite_numbers)),
            winning_numbers(std::move(winning_numbers))
    { }
    
    std::string       firstname;
    std::string       lastname;
    int               age;
    std::set<long>    favorite_numbers;
    std::vector<long> winning_numbers;
    
    bool operator==(const person& other) const
    {
        return std::tie(firstname,       lastname,       age,       favorite_numbers,       winning_numbers)
            == std::tie(other.firstname, other.lastname, other.age, other.favorite_numbers, other.winning_numbers);
    }
    
    friend std::ostream& operator<<(std::ostream& os, const person& p)
    {
        return os << p.firstname << " " << p.lastname << " (" << p.age << ")";
    }
    
    friend std::string to_string(const person& p)
    {
        std::ostringstream os;
        os << p;
        return os.str();
    }
};

}

TEST(serialization_builder_members)
{
    formats base = formats_builder()
                    .type<person>()
                        .member("firstname", &person::firstname)
                        .member("lastname",  &person::lastname)
                        .member("age",       &person::age)
                    .check_references(formats::defaults())
                ;
    formats fmt = formats::compose({ base, formats::defaults() });
    
    person p("Bob", "Builder", 29);
    to_string(p);
    value expected = object({ { "firstname", p.firstname },
                              { "lastname",  p.lastname  },
                              { "age",       p.age       }
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
    
    formats base =
        formats_builder()
            .type<my_pair>()
                .member("a", &my_pair::first)
                .member("b", &my_pair::second)
                    .since({ 2, 0 })
        ;
    formats fmt = formats::compose({ base, formats::defaults() });
    
    auto to_json_ver = [&fmt] (const version& v)
                       {
                        serialization_context context(fmt, v);
                        return context.encode(my_pair(5, 10));
                       };
    
    ensure_eq(0U, to_json_ver({ 1, 0 }).count("b"));
    ensure_eq(1U, to_json_ver({ 2, 0 }).count("b"));
    ensure_eq(1U, to_json_ver({ 3, 0 }).count("b"));
}

TEST(serialization_builder_container_members)
{
    formats base = formats_builder()
                    .type<person>()
                        .member("firstname",        &person::firstname)
                        .member("lastname",         &person::lastname)
                        .member("age",              &person::age)
                        .member("favorite_numbers", &person::favorite_numbers)
                        .member("winning_numbers",  &person::winning_numbers)
                    .register_containers<long, std::set, std::vector>()
                    .check_references(formats::defaults())
                ;
    formats fmt = formats::compose({ base, formats::defaults() });
    
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


TEST(serialization_builder_defaults)
{
    formats base = formats_builder()
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
                    .register_containers<long, std::set, std::vector>()
                    .check_references(formats::defaults())
                ;
    formats fmt = formats::compose({ base, formats::defaults() });
    
    person p("Bob", "Builder", 20, { 1, 2, 3, 4 }, { 1, 2, 3, 4 });
    value input = object({ { "firstname",        p.firstname          },
                           { "lastname",         p.lastname           },
                           { "favorite_numbers", array({ 1, 2, 3, 4 })},
                           { "winning_numbers",  nullptr              },
                         }
                        );
    auto encoded = to_json(p, fmt);
    person q = extract<person>(encoded, fmt);
    ensure_eq(p, q);
}

TEST(serialization_builder_encode_checks)
{
    formats base = formats_builder()
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
                    .check_references(formats::defaults())
                ;
    formats fmt = formats::compose({ base, formats::defaults() });
    
    person p("Bob", "Builder", 20, {}, { 1 });
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
    ensure_throws(std::logic_error, builder.check_references(formats()));
}

}
