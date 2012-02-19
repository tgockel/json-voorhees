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
#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <map>
#include <string>

#include <json-voorhees/value.hpp>
#include <json-voorhees/parse.hpp>

#include "test.hpp"

TEST(demo)
{
    std::string src = "{ \"blazing\": [ 3 \"\\\"\\n\" 4.5 5.123 4.10921e19 ] " // <- no need for commas
                      "  \"text\": [ 1, 2, ,,3,, 4,,, \t\"something\"],,  ,, " // <- or go crazy with commas
                      "  \"we call him \\\"empty array\\\"\": [] "
                      "  \"we call him \\\"empty object\\\"\": {} "
                      "  \"unicode\" :\"\\uface\""
                      "}";
    jsonv::value parsed = jsonv::parse(src);
    std::cout << parsed;
}

int main(int, char**)
{
    int fail_count = 0;
    for (auto iter = jsonv_test::get_unit_tests().begin(); iter != jsonv_test::get_unit_tests().end(); ++iter)
    {
        if (!(*iter)->run())
            ++fail_count;
    }
    
    return fail_count;
}
