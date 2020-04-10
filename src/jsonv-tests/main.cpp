/** \file
 *  
 *  Copyright (c) 2012-2016 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <algorithm>
#include <cassert>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <jsonv/value.hpp>
#include <jsonv/parse.hpp>

#include "filesystem_util.hpp"
#include "test.hpp"

TEST(demo)
{
    std::string src = "{ \"blazing\": [ 3, \"\\\"\\n\", 4.5, 5.123, 4.10921e19 ], "
                      "  \"text\": [ 1, 2, 3, 4, \t\"something\"],  "
                      "  \"we call him \\\"empty array\\\"\": [], "
                      "  \"we call him \\\"empty object\\\"\": {}, "
                      "  \"unicode\" :\"\\uface\""
                      "}";
    jsonv::value parsed = jsonv::parse(src);
    std::cout << parsed;
}

int main(int argc, char** argv)
{
    std::string filter;
    if (argc == 2)
        filter = argv[1];

    std::deque<const jsonv_test::unit_test*> failed_tests;
    for (auto test : jsonv_test::get_unit_tests())
    {
        bool shouldrun = filter.empty()
                      || test->name().find(filter) != std::string::npos;
        if (shouldrun && !test->run())
            failed_tests.push_back(test);
    }

    if (failed_tests.empty())
    {
        return 0;
    }
    else
    {
        std::ostringstream os;
        os << "Unit test failure:" << std::endl;
        for (auto test : failed_tests)
            os << " - " << test->name() << std::endl;

        #ifndef _MSC_VER
        std::cerr << os.str();
        #endif
        return 1;
    }

}
