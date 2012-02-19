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
#ifndef __JSON_VOORHEES_DETAIL_HPP_INCLUDED__
#define __JSON_VOORHEES_DETAIL_HPP_INCLUDED__

#include <json-voorhees/value.hpp>

#include <deque>
#include <map>

namespace jsonv
{
namespace detail
{

template <typename T>
struct cloneable
{
    T* clone() const
    {
        return new T(*static_cast<const T*>(this));
    }
};

class object_impl :
        public cloneable<object_impl>
{
public:
    typedef std::map<string_type, jsonv::value> map_type;
    
    map_type _values;
};

class array_impl :
        public cloneable<array_impl>
{
public:
    typedef std::deque<jsonv::value> array_type;
    
    array_type _values;
};

class string_impl :
        public cloneable<string_impl>
{
public:
    string_type _string;
};

}

const char* kind_desc(kind type);
bool kind_valid(kind k);
void check_type(kind expected, kind actual);

}

#endif/*__JSON_VOORHEES_DETAIL_HPP_INCLUDED__*/
