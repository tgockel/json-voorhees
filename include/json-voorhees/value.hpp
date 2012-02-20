/** \file json-voorhees/value.hpp
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSON_VOORHEES_VALUE_HPP_INCLUDED__
#define __JSON_VOORHEES_VALUE_HPP_INCLUDED__

#include "standard.hpp"

#include <cstdint>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace jsonv
{

namespace detail
{

class object_impl;
class array_impl;
class string_impl;

union value_storage
{
    object_impl* object;
    array_impl*  array;
    string_impl* string;
    int64_t      integer;
    double       decimal;
    bool         boolean;
};

}

enum class kind
{
    null,
    object,
    array,
    string,
    integer,
    decimal,
    boolean
};

class kind_error :
        public std::logic_error
{
public:
    explicit kind_error(const std::string& description);
    
    virtual ~kind_error() throw();
};

class object;
class array;

/** \see http://json.org/
**/
class value
{
public:
    /// Default-construct this to null.
    value();
    value(const value& source);
    value(const string_type& val);
    value(const char_type* val);
    value(int64_t val);
    value(double val);
    value(bool val);
    value(const object& obj);
    value(const array& array);
    
    #define JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_PROTO_GENERATOR(type_)              \
        value(type_ val);
    JSON_VOORHEES_INTEGER_ALTERNATES_LIST(JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_PROTO_GENERATOR)
    
    // Don't allow any implicit conversions that we have not specified.
    template <typename T>
    value(T) = delete;
    
    ~value();
    
    /** Copy-assigns \c source to this.
     *  
     *  If an exception is thrown during the copy, it is propagated out. This instance will be set to a null value - any
     *  value will be disposed.
    **/
    value& operator=(const value& source);
    
    value(value&& source);
    
    /** Move-assigns \c source to this.
     *  
     *  Unlike a copy, this will never throw.
    **/
    value& operator=(value&& source);
    
    static object make_object();
    static value make_array();
    
    /// Resets this value to null.
    void clear();
    
    inline kind get_kind() const
    {
        return _kind;
    }
    
    object& as_object();
    const object& as_object() const;
    
    array& as_array();
    const array& as_array() const;
    
    string_type& as_string();
    const string_type& as_string() const;
    
    int64_t& as_integer();
    int64_t  as_integer() const;
    
    double& as_decimal();
    double  as_decimal() const;
    
    bool& as_boolean();
    bool  as_boolean() const;
    
    /** Compares two JSON values for equality. Two JSON values are equal if and only if all of the following conditions
     *  apply:
     *  
     *   1. They have the same valid value for \c type.
     *      - If \c type is invalid (memory corruption), then two JSON values are \e not equal, even if they have been
     *        corrupted in the same way and even if they share \c this (a corrupt object is not equal to itself).
     *   2. The type comparison is also equal:
     *      a. Two null values are always equivalent.
     *      b. string, integer, decimal and boolean follow the classic rules for their type.
     *      c. objects are equal if they have the same keys and values corresponding with the same key are also equal.
     *      d. arrays are equal if they have the same length and the values at each index are also equal.
     *  
     *  \note
     *  The rules for equality are based on Python \c dict.
    **/
    bool operator ==(const value& other) const;
    
    /** Compares two JSON values for inequality. The rules for inequality are the exact opposite of equality.
    **/
    bool operator !=(const value& other) const;
    
    // FUTURE: I guess do it just like Python?
    // bool operator <(const value& other) const;
    
    friend ostream_type& operator <<(ostream_type& stream, const value& val);
    
protected:
    detail::value_storage _data;
    kind                  _kind;
};

}

#endif/*__JSON_VOORHEES_VALUE_HPP_INCLUDED__*/
