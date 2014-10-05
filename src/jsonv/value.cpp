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
#include <jsonv/encode.hpp>

#include "array.hpp"
#include "char_convert.hpp"
#include "detail.hpp"
#include "object.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ostream>
#include <sstream>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// kind                                                                                                               //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const kind& k)
{
    switch (k)
    {
        case kind::array:   return os << "array";
        case kind::boolean: return os << "boolean";
        case kind::decimal: return os << "decimal";
        case kind::integer: return os << "integer";
        case kind::null:    return os << "null";
        case kind::object:  return os << "object";
        case kind::string:  return os << "string";
        default:            return os << "kind(" << static_cast<int>(k) << ")";
    }
}

std::string to_string(const kind& k)
{
    std::ostringstream ss;
    ss << k;
    return ss.str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// kind_error                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

kind_error::kind_error(const std::string& description) :
        std::logic_error(description)
{ }

kind_error::~kind_error() noexcept
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
    _data.string = new detail::string_impl;
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

value::value(float val) :
        value(double(val))
{ }

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

value::~value() noexcept
{
    clear();
}

value::value(const value& other) :
        _kind(other._kind)
{
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
}

value& value::operator=(const value& other)
{
    value copy(other);
    swap(copy);
    return *this;
}

value::value(value&& other) noexcept :
        _data(other._data),
        _kind(other._kind)
{
    other._data.object = 0;
    other._kind = kind::null;
}

value& value::operator=(value&& source) noexcept
{
    clear();
    
    _data = source._data;
    _kind = source._kind;
    source._data.object = 0;
    source._kind = kind::null;
    
    return *this;
}

void value::swap(value& other) noexcept
{
    using std::swap;
    
    // We swap the objects here because all types of the union a trivially swappable and swap needs to work on a type,
    // not a union.
    static_assert(sizeof _data == sizeof _data.object, "!!");
    swap(_data.object, other._data.object);
    swap(_kind, other._kind);
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

static int decimal_compare(const double& x, const double& y)
{
    return (std::abs(x - y) < (std::numeric_limits<double>::denorm_min() * 10.0)) ?  0
         : (x < y)                                                                ? -1
         :                                                                           1;
}

bool value::operator==(const value& other) const
{
    if (this == &other && kind_valid(get_kind()))
        return true;
    else
        return compare(other) == 0;
}

bool value::operator !=(const value& other) const
{
    // must be first: an invalid type is not equal to itself
    if (!kind_valid(get_kind()))
        return true;
    
    if (this == &other)
        return false;
    else
        return compare(other) != 0;
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
        return decimal_compare(as_decimal(), other.as_decimal());
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

std::ostream& operator<<(std::ostream& stream, const value& val)
{
    ostream_encoder encoder(stream);
    encoder.encode(val);
    return stream;
}

std::string to_string(const value& val)
{
    std::ostringstream os;
    os << val;
    return os.str();
}

bool value::empty() const
{
    check_type({ kind::object, kind::array, kind::string }, get_kind());
    
    switch (get_kind())
    {
    case kind::object:
        return _data.object->empty();
    case kind::array:
        return _data.array->empty();
    case kind::string:
        return _data.string->_string.empty();
    case kind::integer:
    case kind::decimal:
    case kind::boolean:
    case kind::null:
    default:
        // Should never hit this...
        return false;
    }
}

value::size_type value::size() const
{
    check_type({ kind::object, kind::array, kind::string }, get_kind());
    
    switch (get_kind())
    {
    case kind::object:
        return _data.object->size();
    case kind::array:
        return _data.array->size();
    case kind::string:
        return _data.string->_string.size();
    case kind::integer:
    case kind::decimal:
    case kind::boolean:
    case kind::null:
    default:
        // Should never hit this...
        return false;
    }
}

void swap(value& a, value& b) noexcept
{
    a.swap(b);
}

}

namespace std
{

template <typename TForwardIterator, typename FHasher>
static std::size_t hash_range(TForwardIterator first, TForwardIterator last, const FHasher& hasher)
{
    std::size_t x = 0;
    for ( ; first != last; ++first)
        x = (x << 1) ^ hasher(*first);
    
    return x;
}

size_t hash<jsonv::value>::operator()(const jsonv::value& val) const noexcept
{
    using namespace jsonv;
    
    switch (val.get_kind())
    {
    case kind::object:
        return hash_range(val.begin_object(), val.end_object(),
                          [] (const value::object_value_type& x) { return std::hash<std::string>()(x.first)
                                                                        ^ std::hash<value>()(x.second);
                                                                 }
                         );
    case kind::array:
        return hash_range(val.begin_array(), val.end_array(), hash<jsonv::value>());
    case kind::string:
        return std::hash<std::string>()(val.as_string());
    case kind::integer:
        return std::hash<std::int64_t>()(val.as_integer());
    case kind::decimal:
        return std::hash<double>()(val.as_decimal());
    case kind::boolean:
        return std::hash<bool>()(val.as_boolean());
    case kind::null:
        return 0x51afb2fe9467d0f7ULL;
    default:
        // Should never hit this...
        return 0ULL;
    }
}

}
