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
#include "filesystem_util.hpp"

#include <jsonv/parse.hpp>
#include <jsonv/util.hpp>
#include <jsonv/value.hpp>

#include <fstream>

namespace jsonv_test
{

using namespace jsonv;

class json_diff_test :
        public unit_test
{
public:
    json_diff_test(const std::string& test_name,
                   value              left,
                   value              right,
                   value              expected_
                  ) :
            unit_test(std::string("diff_test/") + test_name),
            in_left(std::move(left)),
            in_right(std::move(right))
    {
        expected.same  = std::move(expected_.at("same"));
        expected.left  = std::move(expected_.at("left"));
        expected.right = std::move(expected_.at("right"));
    }
    
    virtual void run_impl() override
    {
        diff_result result = diff(in_left, in_right);
        ensure_eq(expected.same,  result.same);
        ensure_eq(expected.left,  result.left);
        ensure_eq(expected.right, result.right);
    }
    
private:
    value       in_left;
    value       in_right;
    diff_result expected;
};

class json_diff_test_initializer
{
public:
    explicit json_diff_test_initializer(const std::string& rootpath)
    {
        recursive_directory_for_each(rootpath, ".json", [this] (const std::string& p)
        {
            value whole = [&p] { std::ifstream in(p.c_str()); return parse(in); }();
            
            _tests.emplace_back(new json_diff_test(filename(p),
                                                   whole.at_path(".input.left"),
                                                   whole.at_path(".input.right"),
                                                   whole.at_path(".result")
                                                  )
                               );
        });
    }
    
private:
    std::deque<std::unique_ptr<unit_test>> _tests;
} json_diff_test_initializer_instance("src/jsonv-tests/data/diffs");

}
