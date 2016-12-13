/** \file
 *  
 *  Copyright (c) 2016 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "test.hpp"
#include "chrono_io.hpp"
#include "filesystem_util.hpp"
#include "stopwatch.hpp"

#include <jsonv/parse.hpp>
#include <jsonv/util.hpp>
#include <jsonv/value.hpp>

#include <fstream>
#include <iostream>

namespace jsonv_test
{

using namespace jsonv;

static const unsigned iterations = JSONV_DEBUG ? 1 : 100;

template <typename FLoadSource>
static void run_test(FLoadSource load, const std::string& from)
{
    stopwatch timer;
    for (unsigned cnt = 0; cnt < iterations; ++cnt)
    {
        auto src_data = load(from);
        {
            JSONV_TEST_TIME(timer);
            parse(src_data);
        }
    }
    std::cout << timer.get();
}

template <typename FLoadSource>
class benchmark_test :
        public unit_test
{
public:
    benchmark_test(FLoadSource load_source, std::string source_desc, std::string path) :
            unit_test(std::string("benchmark/") + source_desc + "/" + filename(path)),
            load_source(std::move(load_source)),
            path(std::move(path))
    { }

    virtual void run_impl() override
    {
        run_test(load_source, path);
    }

private:
    FLoadSource load_source;
    std::string path;
};

class benchmark_test_initializer
{
public:
    explicit benchmark_test_initializer(const std::string& rootpath)
    {
        using ifstream_loader = std::ifstream (*)(const std::string&);
        ifstream_loader ifstream_load = [] (const std::string& p) { return std::ifstream(p); };

        using string_loader = std::string (*)(const std::string&);
        string_loader string_load = load_from_file;

        recursive_directory_for_each(rootpath, ".json", [&, this] (const std::string& path)
        {
            _tests.emplace_back(new benchmark_test<ifstream_loader>(ifstream_load, "ifstream", path));
            _tests.emplace_back(new benchmark_test<string_loader>(string_load, "string", path));
        });
    }

    static std::string load_from_file(const std::string& path)
    {
        std::ifstream inputfile(path.c_str());
        std::string out;
        inputfile.seekg(0, std::ios::end);
        out.reserve(inputfile.tellg());
        inputfile.seekg(0, std::ios::beg);

        out.assign(std::istreambuf_iterator<char>(inputfile), std::istreambuf_iterator<char>());
        return out;
    }

private:
    std::deque<std::unique_ptr<unit_test>> _tests;
} benchmark_test_initializer_instance(test_path(""));

}
