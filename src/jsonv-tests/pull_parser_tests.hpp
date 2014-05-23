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
#ifndef __JSONV_TESTS_PULL_PARSER_TESTS_HPP_INCLUDED__
#define __JSONV_TESTS_PULL_PARSER_TESTS_HPP_INCLUDED__

#include <jsonv/parse.hpp>

namespace jsonv_test
{

/** Performs pull parsing for the given \e input string. This goes through the \c jsonv::parse overload which takes an
 *  \c std::istream, which goes through the pull parser, but looks like the overload which takes a \c std::string.
**/
jsonv::value pull_parse(const std::string& input, const jsonv::parse_options&);

}

#endif/*__JSONV_TESTS_PULL_PARSER_TESTS_HPP_INCLUDED__*/
