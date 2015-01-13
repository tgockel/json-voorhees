/** \file
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"

#include <jsonv/encode.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/value.hpp>

#include <iostream>
#include <locale>
#include <sstream>

static const char k_some_json[] = R"({
  "a": [ 4, 5, 6, [7, 8, 9, {"something": 5, "else": 6}]],
  "b": "blah",
  "c": { "baz": ["bazar"], "cat": ["Eric", "Bob"] },
  "d": {},
  "e": []
})";

TEST(encode_pretty_print)
{
    auto val = jsonv::parse(k_some_json);
    jsonv::ostream_pretty_encoder encoder(std::cout);
    encoder.encode(val);
}

TEST(encode_locale_comma_numbers)
{
    std::ostringstream os;
    try
    {
        os.imbue(std::locale("de_DE.utf8"));
    }
    catch (std::runtime_error&)
    {
        // skip the test if German isn't available...
        return;
    }
    jsonv::value val = 3.14;
    os << val;
    std::string encoded = os.str();
    jsonv::value decoded = jsonv::parse(encoded);
    ensure_eq(val, decoded);
}
