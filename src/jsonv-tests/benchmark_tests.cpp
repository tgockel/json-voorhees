/** \file
 *
 *  Copyright (c) 2016-2019 by Travis Gockel. All rights reserved.
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

#include <jsonv/algorithm.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/parse_index.hpp>
#include <jsonv/value.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace jsonv_test
{

using namespace jsonv;

static const unsigned iterations = JSONV_DEBUG ? 1 : 100;

template <typename THolster, typename FLoader>
static void run_test(FLoader load, const std::string& from)
{
    stopwatch timer;
    for (unsigned cnt = 0; cnt < iterations; ++cnt)
    {
        THolster src_data{load(from)};
        {
            JSONV_TEST_TIME(timer);
            parse(src_data);
        }
    }
    std::cout << timer.get();
}

// Times stage-1 only: parse_index::parse on a string source. Skips the extract phase
// (jsonv::value materialization), so the number reflects the parse_index hot path alone.
static void run_parse_index_test(const std::string& path)
{
    std::ifstream inputfile(path.c_str());
    std::string src;
    inputfile.seekg(0, std::ios::end);
    src.reserve(inputfile.tellg());
    inputfile.seekg(0, std::ios::beg);
    src.assign(std::istreambuf_iterator<char>(inputfile), std::istreambuf_iterator<char>());

    stopwatch timer;
    for (unsigned cnt = 0; cnt < iterations; ++cnt)
    {
        JSONV_TEST_TIME(timer);
        auto idx = parse_index::parse(src);
        (void)idx;
    }
    std::cout << timer.get();
}

template <typename THolster>
class benchmark_test :
        public unit_test
{
public:
    using loader = std::string (*)(const std::string&);

public:
    benchmark_test(loader load, std::string source_desc, std::string path) :
            unit_test(std::string("benchmark/") + source_desc + "/" + filename(path)),
            load(load),
            path(std::move(path))
    { }

    virtual void run_impl() override
    {
        run_test<THolster>(load, path);
    }

private:
    loader      load;
    std::string path;
};

class parse_index_benchmark_test :
        public unit_test
{
public:
    explicit parse_index_benchmark_test(std::string path) :
            unit_test(std::string("benchmark/parse_index/") + filename(path)),
            path(std::move(path))
    { }

    virtual void run_impl() override
    {
        run_parse_index_test(path);
    }

private:
    std::string path;
};

class benchmark_test_initializer
{
public:
    explicit benchmark_test_initializer(const std::string& rootpath)
    {
        recursive_directory_for_each(rootpath, ".json", [&, this] (const std::string& path)
        {
            if (path.find("fail") == std::string::npos)
            {
                _tests.emplace_back(new benchmark_test<std::ifstream>([] (const std::string& p) { return p; },
                                                                      "ifstream",
                                                                      path
                                                                     )
                                   );
                _tests.emplace_back(new benchmark_test<std::string>(load_from_file, "string", path));
                _tests.emplace_back(new parse_index_benchmark_test(path));
            }
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

// Times the two ways of putting a value into an object against each other. Issue #152 was that `insert` deep-copied
// the pair it was handed, which made it arbitrarily slower than `operator[]` as the inserted value grew; the two
// should now cost about the same.
static void run_object_insert_test(bool use_insert)
{
    constexpr std::size_t entries = 200;

    // Nested, so that a deep copy of it is much more expensive than moving the handle.
    value payload = object();
    for (int idx = 0; idx < 64; ++idx)
        payload[std::to_string(idx)] = array({ idx, "string payload", double(idx) * 1.5 });

    std::vector<std::string> keys;
    for (std::size_t idx = 0; idx < entries; ++idx)
        keys.emplace_back("key-" + std::to_string(idx));

    stopwatch timer;
    for (unsigned cnt = 0; cnt < iterations; ++cnt)
    {
        std::vector<value> sources(entries, payload);
        value              dst = object();
        {
            JSONV_TEST_TIME(timer);
            for (std::size_t idx = 0; idx < entries; ++idx)
            {
                if (use_insert)
                    dst.insert({ keys[idx], std::move(sources[idx]) });
                else
                    dst[keys[idx]] = std::move(sources[idx]);
            }
        }
    }
    std::cout << timer.get();
}

class object_insert_benchmark_test :
        public unit_test
{
public:
    object_insert_benchmark_test(const std::string& name, bool use_insert) :
            unit_test("benchmark/object_insert/" + name),
            use_insert(use_insert)
    { }

    virtual void run_impl() override
    {
        run_object_insert_test(use_insert);
    }

private:
    bool use_insert;
};

object_insert_benchmark_test object_insert_benchmark_insert_instance("insert", true);
object_insert_benchmark_test object_insert_benchmark_subscript_instance("subscript", false);

}
