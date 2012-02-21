/** \file
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <json-voorhees/value.hpp>
#include <json-voorhees/parse.hpp>

#include "test.hpp"

TEST(parse_empty_string)
{
    jsonv::value val = jsonv::parse("\"\"");
    const std::string& str = val.as_string();
    ensure(str == "");
}

TEST(parse_single_backslash)
{
    jsonv::value val = jsonv::parse("\"\\\\\""); // "\\"
    const std::string& str = val.as_string();
    ensure(str == "\\");
}
