/// \file
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "core.hpp"

#include <jsonv/all.hpp>

namespace json_benchmark
{

class jsonv_benchmark_suite :
        public typed_benchmark_suite<jsonv::value>
{
public:
    jsonv_benchmark_suite() :
            typed_benchmark_suite<jsonv::value>("JSONV")
    { }

protected:
    virtual jsonv::value parse(const std::string& source) const
    {
        return jsonv::parse(source);
    }

} jsonv_benchmark_suite_instance;

class jsonv_ast_benchmark_suite :
        public typed_benchmark_suite<jsonv::parse_index>
{
public:
    jsonv_ast_benchmark_suite() :
            typed_benchmark_suite<jsonv::parse_index>("JSONV-AST")
    { }

protected:
    virtual jsonv::parse_index parse(const std::string& source) const
    {
        return jsonv::parse_index::parse(source);
    }
} jsonv_ast_benchmark_suite_instance;

}
