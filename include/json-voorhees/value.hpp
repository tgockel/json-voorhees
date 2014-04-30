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

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <iterator>
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
    value(std::nullptr_t);
    value(const value& source);
    value(const std::string& val);
    value(const char* val);
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
    static array make_array();
    
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
    
    std::string& as_string();
    const std::string& as_string() const;
    
    int64_t& as_integer();
    int64_t  as_integer() const;
    
    /** Get this object as a decimal. There is interesting behavior if the object's underlying kind is actually an
     *  integer type. The non-const version of this function will alter the kind to \c kind::decimal before returning a
     *  reference. The const version does not alter the kind, but casts the integer to double.
    **/
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
    bool operator!=(const value& other) const;
    
    /** Used to build a strict-ordering of JSON values. When comparing values of the same kind, the ordering should
     *  align with your intuition. When comparing values of different kinds, some arbitrary rules were created based on
     *  how "complicated" the author thought the type to be.
     *  
     *  null - less than everything but null, which it is equal to.
     *  boolean - false is less than true.
     *  integer, decimal - compared by their numeric value. Comparisons between two integers do not cast, but comparison
     *    between an integer and a decimal will coerce to decimal.
     *  string - compared lexicographically by character code (with basic char strings and non-ASCII encoding, this
     *    might lead to surprising results)
     *  array - compared lexicographically by elements (recursively following this same technique)
     *  object - entries in the object are sorted and compared lexicographically, first by key then by value
     *  
     *  \returns -1 if this is less than other by the rules stated above; 0 if this is equal to other; -1 if otherwise.
    **/
    int compare(const value& other) const;
    
    bool operator< (const value& other) const;
    bool operator> (const value& other) const;
    bool operator<=(const value& other) const;
    bool operator>=(const value& other) const;
    
    friend std::ostream& operator <<(std::ostream& stream, const value& val);
    
protected:
    detail::value_storage _data;
    kind                  _kind;
};

}

#endif/*__JSON_VOORHEES_VALUE_HPP_INCLUDED__*/
