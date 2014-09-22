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
    explicit json_checker_test(const fs::path& datapath) :
            unit_test(std::string("json_checker_test/") + datapath.filename().string()),
            _datapath(datapath)
    { }
    
private:
    virtual void run_impl() override
    {
        bool expect_failure = _datapath.filename().string().find("fail") == 0;
        std::ifstream file(_datapath.c_str());
        if (expect_failure)
        {
            ensure_throws(jsonv::parse_error, jsonv::parse(file));
        }
        else
        {
            jsonv::parse(file);
        }
    }
    
private:
    fs::path _datapath;
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
                std::unique_ptr<json_checker_test> test(new json_checker_test(p));
                _tests.emplace_back(std::move(test));
            }
        }
    }
    
private:
    std::deque<std::unique_ptr<json_checker_test>> _tests;
} json_checker_test_initializer_instance("src/jsonv-tests/data/json_checker");

}
