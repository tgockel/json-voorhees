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
#ifndef __TEST_JSON_VOORHEES_TEST_HPP_INCLUDED__
#define __TEST_JSON_VOORHEES_TEST_HPP_INCLUDED__

#include <deque>
#include <string>

namespace jsonv_test
{

class unit_test;

typedef std::deque<unit_test*> unit_test_list_type;
unit_test_list_type& get_unit_tests();

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
    explicit unit_test(const std::string& name);
    
    bool run();
    
private:
    virtual void run_impl() = 0;
    
protected:
    std::string _name;
    bool        _success;
    std::string _failstring;
};

#define TEST(name_) \
    class name_ ## _test :                   \
            public ::jsonv_test::unit_test   \
    {                                        \
    public:                                  \
        name_ ## _test() :                   \
            ::jsonv_test::unit_test(#name_)  \
        { }                                  \
                                             \
        void run_impl();                     \
    } name_ ## _test_instance;               \
                                             \
    void name_ ## _test::run_impl()

}

#endif/*__TEST_JSON_VOORHEES_TEST_HPP_INCLUDED__*/
