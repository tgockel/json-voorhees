/** \file
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <map>
#include <string>

#include <json-voorhees/value.hpp>
#include <json-voorhees/parse.hpp>

class unit_test;

std::deque<unit_test*> unit_tests;

#define ASSERT_ON_TEST_FAILURE 0
#if ASSERT_ON_TEST_FAILURE
#   define ensure assert
#else
#   define ensure(cond_)                    \
        do                                  \
        {                                   \
            if (!(cond_))                   \
            {                               \
                this->_success = false;     \
                this->_failstring = #cond_; \
                return;                     \
            }                               \
        } while (0)
#endif

class unit_test
{
public:
    explicit unit_test(const std::string& name) :
            _name(name)
    {
        unit_tests.push_back(this);
    }
    
    bool run()
    {
        std::cout << "TEST: " << _name << " ...";
        _success = true;
        try
        {
            run_impl();
        }
        catch (const std::exception& ex)
        {
            _success = false;
            _failstring = std::string("Threw exception: ") + ex.what();
        }
        if (_success)
            std::cout << " SUCCESS!" << std::endl;
        else
            std::cout << " \x1b[0;31mFAILURE " << _failstring << "\x1b[m" << std::endl;
        return _success;
    }
    
private:
    virtual void run_impl() = 0;
    
protected:
    std::string _name;
    bool        _success;
    std::string _failstring;
};

#define TEST(name_) \
    class name_ ## _test : public unit_test  \
    {                                        \
    public:                                  \
        name_ ## _test() :                   \
            unit_test(#name_)                \
        { }                                  \
                                             \
        void run_impl();                     \
    } name_ ## _test_instance;               \
                                             \
    void name_ ## _test::run_impl()

TEST(array)
{
    jsonv::value val = jsonv::make_array();
    jsonv::array_view arr = val.as_array();
    arr.push_back(8.9);
    ensure(arr.size() == 1);
    ensure(arr[0].type() == jsonv::value_type::decimal);
    arr.push_back(true);
    ensure(arr.size() == 2);
    ensure(arr[0].type() == jsonv::value_type::decimal);
    ensure(arr[1].type() == jsonv::value_type::boolean);
    arr[0] = "Hi";
    ensure(arr.size() == 2);
    ensure(arr[0].type() == jsonv::value_type::string);
    ensure(arr[1].type() == jsonv::value_type::boolean);
}

TEST(make_array)
{
    jsonv::value val = jsonv::make_array(2, 10, "Hello, world!");
    ensure(val.type() == jsonv::value_type::array);
    jsonv::array_view arr = val.as_array();
    ensure(arr.size() == 3);
    ensure(arr[0].as_integer() == 2);
    ensure(arr[1].as_integer() == 10);
    ensure(arr[2].as_string() == "Hello, world!");
}

TEST(object)
{
    jsonv::value val = jsonv::make_object();
    jsonv::object_view obj = val.as_object();
    obj["hi"] = false;
    ensure(obj["hi"].as_boolean() == false);
    ensure(val.as_object()["hi"].as_boolean() == false);
    obj["yay"] = jsonv::make_array("Hello", "to", "the", "world");
    ensure(obj["hi"].as_boolean() == false);
    ensure(val.as_object()["hi"].as_boolean() == false);
    ensure(obj["yay"].as_array().size() == 4);
    ensure(val.as_object()["yay"].as_array().size() == 4);
}

TEST(parse_array)
{
    jsonv::value val = jsonv::parse("\t\n[2, 10, \"Hello, world!\"]   ");
    ensure(val.type() == jsonv::value_type::array);
    jsonv::array_view arr = val.as_array();
    ensure(arr.size() == 3);
    ensure(arr[0].as_integer() == 2);
    ensure(arr[1].as_integer() == 10);
    ensure(arr[2].as_string() == "Hello, world!");
    ensure(val == jsonv::make_array(2, 10, "Hello, world!"));
}

TEST(parse_unicode_single)
{
    std::string s = jsonv::parse("\"\\u0004\"").as_string();
    ensure(s.size() == 1);
    ensure(s[0] == '\x04');
}

TEST(parse_unicode_inline)
{
    std::string s = jsonv::parse("\"é\"").as_string();
    ensure(s.size() == 2);
    ensure(s[0] == '\xc3');
    ensure(s[1] == '\xa9');
}

TEST(parse_unicode_multi)
{
    std::string s = jsonv::parse("\"\\u00e9\"").as_string();
    ensure(s.size() == 2);
    ensure(s[0] == '\xc3');
    ensure(s[1] == '\xa9');
}

TEST(parse_unicode_insanity)
{
    std::string s = jsonv::parse("\"\\uface\"").as_string();
    ensure(s.size() == 3);
    // The right answer according to Python: u'\uface'.encode('utf-8')
    const char vals[] = "\xef\xab\x8e";
    for (unsigned idx = 0; idx < 3; ++idx)
        ensure(s[idx] == vals[idx]);
}

TEST(parse_unicode_invalid_surrogates)
{
    // This is technically an invalid Unicode because of some unpaired surrogate...apparently 0xd800 - 0xdfff is a magic
    // range for something I don't understand. Anyway, this library will parse and translate it to UTF8 (perhaps not in
    // a valid way). My shell doesn't seem to have a problem printing a ? for this.
    std::string s = jsonv::parse("\"\\udead\\ubeef\"").as_string();
    ensure(s.size() == 6);
    // The right answer according to Python: u'\udead\ubeef'.encode('utf-8')
    const char vals[] = "\xed\xba\xad\xeb\xbb\xaf";
    for (unsigned idx = 0; idx < sizeof vals; ++idx)
        ensure(s[idx] == vals[idx]);
}

TEST(object_view_iter_assign)
{
    using namespace jsonv;
    
    value val = make_object("foo", 5, "bar", "wat");
    std::map<std::string, bool> found{ { "foo", false }, { "bar", false } };
    object_view obj = val.as_object();
    ensure(obj.size() == 2);
    
    for (object_view::const_iterator iter = obj.begin(); iter != obj.end(); ++iter)
        found[iter->first] = true;
    
    for (auto iter = found.begin(); iter != found.end(); ++iter)
        ensure(iter->second);
}

TEST(array_view_iter_assign)
{
    using namespace jsonv;
    
    value val = make_array(0, 1, 2, 3, 4, 5);
    array_view arr = val.as_array();
    int64_t i = 0;
    for (array_view::const_iterator iter = arr.begin(); iter != arr.end(); ++iter)
    {
        ensure(iter->as_integer() == i);
        ++i;
    }
}

TEST(demo)
{
    std::string src = "{ \"blazing\": [ 3 \"\\\"\\n\" 4.5 5.123 4.10921e19 ] " // <- no need for commas
                      "  \"text\": [ 1, 2, ,,3,, 4,,, \t\"something\"],,  ,, " // <- or go crazy with commas
                      "  \"we call him \\\"empty array\\\"\": [] "
                      "  \"we call him \\\"empty object\\\"\": {} "
                      "  \"unicode\" :\"\\uface\""
                      "}";
    jsonv::value parsed = jsonv::parse(src);
    std::cout << parsed;
}

int main(int, char**)
{
    int fail_count = 0;
    for (auto iter = unit_tests.begin(); iter != unit_tests.end(); ++iter)
    {
        if (!(*iter)->run())
            ++fail_count;
    }
    
    return fail_count;
}
