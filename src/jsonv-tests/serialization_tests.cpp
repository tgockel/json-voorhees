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
#include <jsonv/serialization.hpp>
#include <jsonv/value.hpp>

namespace jsonv_test
{

using namespace jsonv;

namespace
{

struct unassociated { };

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
    ensure_eq(5, cxt.extract_sub<std::int8_t>(val, "i"));
    ensure_eq(5, cxt.extract_sub<std::uint8_t>(val, "i"));
    ensure_eq(5, cxt.extract_sub<std::int16_t>(val, "i"));
    ensure_eq(5, cxt.extract_sub<std::uint16_t>(val, "i"));
    ensure_eq(5, cxt.extract_sub<std::int32_t>(val, "i"));
    ensure_eq(5, cxt.extract_sub<std::uint32_t>(val, "i"));
    ensure_eq(5, cxt.extract_sub<std::int64_t>(val, "i"));
    ensure_eq(5, cxt.extract_sub<std::uint64_t>(val, "i"));
    ensure_eq(4.5f, cxt.extract_sub<float>(val, "d"));
    ensure_eq(4.5, cxt.extract_sub<double>(val, "d"));
    ensure_eq("thing", cxt.extract_sub<std::string>(val, "s"));
    ensure_throws(extraction_error, cxt.extract_sub<unassociated>(val, "o"));
    ensure_throws(extraction_error, cxt.extract_sub<int>(val, path::create(".a[3]")));
}

}
