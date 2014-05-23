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
#include "pull_parser_tests.hpp"

#include <sstream>

namespace jsonv_test
{

jsonv::value pull_parse(const std::string& input, const jsonv::parse_options& options)
{
    // not performant, but that's not the point
    std::istringstream is(input);
    return jsonv::parse(is, options);
}

}
