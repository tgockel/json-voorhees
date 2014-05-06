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
#include <jsonv/value.hpp>
#include <jsonv/array.hpp>
#include <jsonv/object.hpp>

#include "char_convert.hpp"
#include "detail.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ostream>
#include <sstream>

using namespace jsonv::detail;

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// kind_error                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

kind_error::kind_error(const std::string& description) :
        std::logic_error(description)
{ }

kind_error::~kind_error() throw()
{ }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// value                                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

value::value() :
        _kind(kind::null)
{
    _data.object = nullptr;
}

value::value(std::nullptr_t) :
        value()
{ }

value::value(const std::string& val) :
        _kind(kind::null)
{
    _data.string = new string_impl;
    _kind = kind::string;
    _data.string->_string = val;
}

value::value(const char* val) :
        value(std::string(val))
{ }

value::value(int64_t val) :
        _kind(kind::integer)
{
    _data.integer = val;
}

value::value(double val) :
        _kind(kind::decimal)
{
    _data.decimal = val;
}

value::value(bool val) :
        _kind(kind::boolean)
{
    _data.boolean = val;
}

#define JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_GENERATOR(type_)              \
    value::value(type_ val) :                                              \
            _kind(kind::integer)                                           \
    {                                                                      \
        _data.integer = val;                                               \
    }
JSONV_INTEGER_ALTERNATES_LIST(JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_GENERATOR)

value::~value() throw()
{
    clear();
}

value& value::operator=(const value& other)
{
    clear();
    
    switch (other.get_kind())
    {
    case kind::object:
        _data.object = other._data.object->clone();
        break;
    case kind::array:
        _data.array = other._data.array->clone();
        break;
    case kind::string:
        _data.string = other._data.string->clone();
        break;
    case kind::integer:
        _data.integer = other._data.integer;
        break;
    case kind::decimal:
        _data.decimal = other._data.decimal;
        break;
    case kind::boolean:
        _data.boolean = other._data.boolean;
        break;
    case kind::null:
        break;
    }
    
    // it is important that we set our type AFTER we copy the data so we're in a consistent state if the copy throws
    _kind = other._kind;
    
    return *this;
}

value::value(const value& other) :
        _kind(kind::null)
{
    // just re-use assignment operator
    *this = other;
}

value::value(value&& other) throw() :
        _data(other._data),
        _kind(other._kind)
{
    other._data.object = 0;
    other._kind = kind::null;
}

value& value::operator =(value&& source) throw()
{
    clear();
    
    _data = source._data;
    _kind = source._kind;
    source._data.object = 0;
    source._kind = kind::null;
    
    return *this;
}

void value::clear()
{
    switch (_kind)
    {
    case kind::object:
        delete _data.object;
        break;
    case kind::array:
        delete _data.array;
        break;
    case kind::string:
        delete _data.string;
        break;
    case kind::integer:
    case kind::decimal:
    case kind::boolean:
    case kind::null:
        // do nothing
        break;
    }
    
    _kind = kind::null;
    _data.object = 0;
}

const std::string& value::as_string() const
{
    check_type(kind::string, _kind);
    return _data.string->_string;
}

int64_t value::as_integer() const
{
    check_type(kind::integer, _kind);
    return _data.integer;
}

double value::as_decimal() const
{
    if (_kind == kind::integer)
        return double(as_integer());
    check_type(kind::decimal, _kind);
    return _data.decimal;
}

bool value::as_boolean() const
{
    check_type(kind::boolean, _kind);
    return _data.boolean;
}

bool value::operator==(const value& other) const
{
    if (this == &other && kind_valid(get_kind()))
        return true;
    if (get_kind() != other.get_kind())
        return false;
    
    switch (get_kind())
    {
    case kind::object:
        return *_data.object == *_data.object;
    case kind::array:
        return *_data.array == *_data.array;
    case kind::string:
        return as_string() == other.as_string();
    case kind::integer:
        return as_integer() == other.as_integer();
    case kind::decimal:
        return as_decimal() == other.as_decimal();
    case kind::boolean:
        return as_boolean() == other.as_boolean();
    case kind::null:
        return true;
    default:
        return false;
    }
}

bool value::operator !=(const value& other) const
{
    // must be first: an invalid type is not equal to itself
    if (!kind_valid(get_kind()))
        return true;
    
    if (this == &other)
        return false;
    if (get_kind() != other.get_kind())
        return true;
    
    switch (get_kind())
    {
    case kind::object:
        return *_data.object != *_data.object;
    case kind::array:
        return *_data.array != *_data.array;
    case kind::string:
        return as_string() != other.as_string();
    case kind::integer:
        return as_integer() != other.as_integer();
    case kind::decimal:
        return as_decimal() != other.as_decimal();
    case kind::boolean:
        return as_boolean() != other.as_boolean();
    case kind::null:
        return false;
    default:
        return true;
    }
}

static int kindval(kind k)
{
    switch (k)
    {
    case kind::null:
        return 0;
    case kind::boolean:
        return 1;
    case kind::integer:
    case kind::decimal:
        return 2;
    case kind::string:
        return 3;
    case kind::array:
        return 4;
    case kind::object:
        return 5;
    default:
        return -1;
    }
}

static int compare_kinds(kind a, kind b)
{
    int va = kindval(a);
    int vb = kindval(b);
    return va == vb ? 0 : va < vb ? -1 : 1;
}

int value::compare(const value& other) const
{
    if (this == &other)
        return 0;
    
    if (int kindcmp = compare_kinds(get_kind(), other.get_kind()))
        return kindcmp;
    
    switch (get_kind())
    {
    case kind::null:
        return 0;
    case kind::boolean:
        return as_boolean() == other.as_boolean() ? 0 : as_boolean() ? 1 : -1;
    case kind::integer:
        // other might be a decimal type, but if they are both integers, compare directly
        if (other.get_kind() == kind::integer)
            return as_integer() == other.as_integer() ? 0 : as_integer() < other.as_integer() ? -1 : 1;
        // fall through
    case kind::decimal:
        return as_decimal() == other.as_decimal() ? 0 : as_decimal() < other.as_decimal() ? -1 : 1;
    case kind::string:
        return as_string().compare(other.as_string());
    case kind::array:
        return _data.array->compare(*other._data.array);
    case kind::object:
        return _data.object->compare(*other._data.object);
    default:
        return -1;
    }
}

bool value::operator< (const value& other) const
{
    return compare(other) == -1;
}

bool value::operator<=(const value& other) const
{
    return compare(other) != 1;
}

bool value::operator> (const value& other) const
{
    return compare(other) == 1;
}

bool value::operator>=(const value& other) const
{
    return compare(other) != -1;
}

std::ostream& operator <<(std::ostream& stream, const value& val)
{
    switch (val.get_kind())
    {
    case kind::object:
        return stream << *val._data.object;
    case kind::array:
        return stream << *val._data.array;
    case kind::string:
        return stream_escaped_string(stream, val.as_string());
    case kind::integer:
        return stream << val.as_integer();
    case kind::decimal:
        if (std::isnormal(val.as_decimal()))
            return stream << val.as_decimal();
        // not a number isn't a valid JSON value, so put it as null
        else
            return stream << "null";
    case kind::boolean:
        return stream << (val.as_boolean() ? "true" : "false");
    case kind::null:
    default:
        return stream << "null";
    }
}

}
