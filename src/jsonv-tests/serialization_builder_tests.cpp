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

#include <sstream>
#include <tuple>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

struct person
{
    person() = default;
    
    person(std::string f, std::string l, int a) :
            firstname(std::move(f)),
            lastname(std::move(l)),
            age(a)
    { }
    
    std::string firstname;
    std::string lastname;
    int         age;
    
    bool operator==(const person& other) const
    {
        return std::tie(firstname, lastname, age) == std::tie(other.firstname, other.lastname, other.age);
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

}
