/// \file
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// The JSON Voorhees adapter for nativejson-benchmark (https://github.com/miloyip/nativejson-benchmark). Upstream's
/// own \c voorheestest.cpp was written against 1.x and includes source files which no longer exist; this one follows
/// the same shape against the current API.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.h"

#include <jsonv/encode.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/value.hpp>

#include <cstring>
#include <sstream>
#include <string>
#include <string_view>

namespace
{

void gen_stat(Stat& stat, const jsonv::value& v)
{
    switch (v.kind())
    {
    case jsonv::kind::array:
        for (const jsonv::value& sub : v.as_array())
            gen_stat(stat, sub);
        stat.arrayCount++;
        stat.elementCount += v.size();
        break;
    case jsonv::kind::object:
        for (const auto& [key, sub] : v.as_object())
        {
            gen_stat(stat, sub);
            stat.stringLength += key.size();
        }
        stat.objectCount++;
        stat.memberCount += v.size();
        stat.stringCount += v.size();
        break;
    case jsonv::kind::string:
        stat.stringCount++;
        stat.stringLength += v.size();
        break;
    case jsonv::kind::integer:
    case jsonv::kind::decimal:
        stat.numberCount++;
        break;
    case jsonv::kind::boolean:
        if (v.as_boolean())
            stat.trueCount++;
        else
            stat.falseCount++;
        break;
    case jsonv::kind::null:
        stat.nullCount++;
        break;
    }
}

// Strict, so the jsonchecker documents which are meant to fail do; see miloyip/nativejson-benchmark#74.
jsonv::value parse_strict(std::string_view text)
{
    return jsonv::parse(text, jsonv::parse_options::create_strict());
}

class jsonv_parse_result :
        public ParseResultBase
{
public:
    jsonv::value root;
};

class jsonv_string_result :
        public StringResultBase
{
public:
    const char* c_str() const override { return s.c_str(); }

    std::string s;
};

class jsonv_test :
        public TestBase
{
public:
#if TEST_INFO
    const char* GetName() const override     { return "JSON Voorhees (C++)"; }
    const char* GetFilename() const override { return __FILE__; }
#endif

#if TEST_PARSE
    ParseResultBase* Parse(const char* json, size_t length) const override
    {
        auto pr = new jsonv_parse_result;
        try
        {
            pr->root = parse_strict(std::string_view(json, length));
        }
        catch (...)
        {
            delete pr;
            return nullptr;
        }
        return pr;
    }
#endif

#if TEST_STRINGIFY
    StringResultBase* Stringify(const ParseResultBase* parseResult) const override
    {
        auto pr = static_cast<const jsonv_parse_result*>(parseResult);
        auto sr = new jsonv_string_result;
        sr->s = jsonv::to_string(pr->root);
        return sr;
    }
#endif

#if TEST_PRETTIFY
    StringResultBase* Prettify(const ParseResultBase* parseResult) const override
    {
        auto pr = static_cast<const jsonv_parse_result*>(parseResult);
        auto sr = new jsonv_string_result;
        std::ostringstream os;
        jsonv::ostream_pretty_encoder encoder(os, 4);
        encoder.encode(pr->root);
        sr->s = std::move(os).str();
        return sr;
    }
#endif

#if TEST_STATISTICS
    bool Statistics(const ParseResultBase* parseResult, Stat* stat) const override
    {
        auto pr = static_cast<const jsonv_parse_result*>(parseResult);
        std::memset(stat, 0, sizeof(Stat));
        gen_stat(*stat, pr->root);
        return true;
    }
#endif

#if TEST_CONFORMANCE
    bool ParseDouble(const char* json, double* d) const override
    {
        try
        {
            *d = parse_strict(json)[0].as_decimal();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ParseString(const char* json, std::string& s) const override
    {
        try
        {
            s = parse_strict(json)[0].as_string();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
#endif
};

}

REGISTER_TEST(jsonv_test);
