/** \file
 *  Data-driven tests for running the samples from http://json.org/JSON_checker/.
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

#include <jsonv/parse.hpp>

#include <fstream>
#include <memory>

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace jsonv_test
{

class json_checker_test :
        public unit_test
{
public:
    explicit json_checker_test(const fs::path&             datapath,
                               const std::string&          test_name_post,
                               bool                        expect_failure,
                               const jsonv::parse_options& options
                              ) :
            unit_test(std::string("json_checker_test/") + datapath.filename().string() + test_name_post),
            _datapath(datapath),
            _expect_failure(expect_failure),
            _options(options)
    { }
    
private:
    virtual void run_impl() override
    {
        std::ifstream file(_datapath.c_str());
        if (_expect_failure)
        {
            ensure_throws(jsonv::parse_error, jsonv::parse(file, _options));
        }
        else
        {
            jsonv::parse(file, _options);
        }
    }
    
private:
    fs::path             _datapath;
    bool                 _expect_failure;
    jsonv::parse_options _options;
};

class json_checker_test_initializer
{
public:
    explicit json_checker_test_initializer(const std::string& rootpathname)
    {
        fs::path rootpath(rootpathname);
        for (fs::directory_iterator iter(rootpath); iter != fs::directory_iterator(); ++iter)
        {
            fs::path p = *iter;
            if (p.extension() == ".json")
            {
                bool expect_failure = p.filename().string().find("fail") == 0;
                std::unique_ptr<json_checker_test> test(new json_checker_test(p,
                                                                              "",
                                                                              expect_failure,
                                                                              jsonv::parse_options::create_default()
                                                                             )
                                                       );
                _tests.emplace_back(std::move(test));
                
                if (p.filename().string().find("pass-but-fail-strict") == 0)
                {
                    std::unique_ptr<json_checker_test> strict(new json_checker_test(p,
                                                                                    "+strict",
                                                                                    true,
                                                                                    jsonv::parse_options::create_strict()
                                                                                   )
                                                             );
                    _tests.emplace_back(std::move(strict));
                }
            }
        }
    }
    
private:
    std::deque<std::unique_ptr<json_checker_test>> _tests;
} json_checker_test_initializer_instance("src/jsonv-tests/data/json_checker");

}
