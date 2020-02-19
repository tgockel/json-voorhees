/// \file
/// Data-driven tests for running the samples from http://json.org/JSON_checker/.
///
/// Copyright (c) 2014-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/ast.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/parse_index.hpp>

#include <fstream>
#include <memory>
#include <string>
#include <streambuf>

#include <jsonv-tests/filesystem_util.hpp>

namespace jsonv_test
{

class json_checker_test :
        public unit_test
{
public:
    explicit json_checker_test(const std::string&          datapath,
                               const std::string&          test_name_post,
                               bool                        expect_failure,
                               const jsonv::parse_options& options
                              ) :
            unit_test(std::string("json_checker_test/") + filename(datapath) + test_name_post),
            _datapath(datapath),
            _expect_failure(expect_failure),
            _options(options)
    { }

private:
    virtual void run_impl() override
    {
        std::ifstream file(_datapath.c_str());
        auto text = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        auto ast  = jsonv::parse_index::parse(text);
        if (_expect_failure == ast.success())
            ast.validate();
    }

private:
    std::string          _datapath;
    bool                 _expect_failure;
    jsonv::parse_options _options;
};

class json_checker_test_initializer
{
public:
    explicit json_checker_test_initializer(const std::string& rootpathname)
    {
        recursive_directory_for_each(rootpathname, ".json", [this] (const std::string& p)
        {
            bool expect_failure = filename(p).find("fail") == 0;
            std::unique_ptr<json_checker_test> test(new json_checker_test(p,
                                                                          "",
                                                                          expect_failure,
                                                                          jsonv::parse_options::create_default()
                                                                         )
                                                   );
            _tests.emplace_back(std::move(test));
        });
    }

private:
    std::deque<std::unique_ptr<json_checker_test>> _tests;
} json_checker_test_initializer_instance(test_path("json_checker"));

}
