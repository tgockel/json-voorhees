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
#include <json-voorhees/value.hpp>

#include "char_convert.hpp"

#include <cstring>
#include <deque>
#include <map>
#include <ostream>
#include <sstream>
#include <string>

using namespace jsonv::detail;

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Implementation Details                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

static ostream_type& stream_escaped_string(ostream_type& stream, const string_type& str)
{
    stream << "\"";
    string_encode(stream, str);
    stream << "\"";
    return stream;
}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// value_type                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char* kind_desc(kind type)
{
    switch (type)
    {
    case kind::object:
        return "object";
    case kind::array:
        return "array";
    case kind::string:
        return "string";
    case kind::integer:
        return "integer";
    case kind::decimal:
        return "decimal";
    case kind::boolean:
        return "boolean";
    case kind::null:
        return "null";
    default:
        return "UNKNOWN";// should never happen
    }
}

static bool kind_valid(kind k)
{
    switch (k)
    {
    case kind::object:
    case kind::array:
    case kind::string:
    case kind::integer:
    case kind::decimal:
    case kind::boolean:
    case kind::null:
        return true;
    default:
        return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// kind_error                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

kind_error::kind_error(const std::string& description) :
        std::logic_error(description)
{ }

kind_error::~kind_error() throw()
{ }

static void check_type(kind expected, kind actual)
{
    if (expected != actual)
    {
        std::ostringstream stream;
        stream << "Unexpected type: expected " << kind_desc(expected)
            << " but found " << kind_desc(actual) << ".";
        throw kind_error(stream.str());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// value                                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

value::value() :
        _kind(kind::null)
{
    _data.object = 0;
}

value::value(const string_type& val) :
        _kind(kind::null)
{
    _data.string = new string_impl;
    _kind = kind::string;
    _data.string->_string = val;
}

value::value(const char_type* val) :
        _kind(kind::null)
{
    _data.string = new string_impl;
    _kind = kind::string;
    _data.string->_string = val;
}

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

value::value(const object_view& obj_view) :
        _kind(kind::null)
{
    _data.object = obj_view._source->clone();
    _kind = kind::object;
}

value::value(const array_view& arr_view) :
        _kind(kind::null)
{
    _data.array = arr_view._source->clone();
    _kind = kind::array;
}

#define JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_GENERATOR(type_)              \
    value::value(type_ val) :                                              \
            _kind(kind::integer)                                           \
    {                                                                      \
        _data.integer = val;                                               \
    }
JSON_VOORHEES_INTEGER_ALTERNATES_LIST(JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_GENERATOR)

value::~value()
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

value::value(value&& other) :
        _data(other._data),
        _kind(other._kind)
{
    other._data.object = 0;
    other._kind = kind::null;
}

value& value::operator =(value&& source)
{
    clear();
    
    _data = source._data;
    _kind = source._kind;
    source._data.object = 0;
    source._kind = kind::null;
    
    return *this;
}

value value::make_object()
{
    value val;
    val._data.object = new object_impl;
    val._kind = kind::object;
    
    return val;
}

value value::make_array()
{
    value val;
    val._data.array = new array_impl;
    val._kind = kind::array;
    
    return val;
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

object_view value::as_object()
{
    check_type(kind::object, _kind);
    return object_view(_data.object);
}

const object_view value::as_object() const
{
    check_type(kind::object, _kind);
    // const_cast is okay, since we're giving back something that won't modify us
    return object_view(const_cast<detail::object_impl*>(_data.object));
}

array_view value::as_array()
{
    check_type(kind::array, _kind);
    return array_view(_data.array);
}

const array_view value::as_array() const
{
    check_type(kind::array, _kind);
    return array_view(const_cast<detail::array_impl*>(_data.array));
}

string_type& value::as_string()
{
    check_type(kind::string, _kind);
    return _data.string->_string;
}

const string_type& value::as_string() const
{
    check_type(kind::string, _kind);
    return _data.string->_string;
}

int64_t& value::as_integer()
{
    check_type(kind::integer, _kind);
    return _data.integer;
}

int64_t value::as_integer() const
{
    check_type(kind::integer, _kind);
    return _data.integer;
}

double& value::as_decimal()
{
    check_type(kind::decimal, _kind);
    return _data.decimal;
}

double value::as_decimal() const
{
    check_type(kind::decimal, _kind);
    return _data.decimal;
}

bool& value::as_boolean()
{
    check_type(kind::boolean, _kind);
    return _data.boolean;
}

bool value::as_boolean() const
{
    check_type(kind::boolean, _kind);
    return _data.boolean;
}

bool value::operator ==(const value& other) const
{
    if (this == &other && kind_valid(kind()))
        return true;
    if (get_kind() != other.get_kind())
        return false;
    
    switch (kind())
    {
    case kind::object:
        return as_object() == other.as_object();
    case kind::array:
        return as_array() == other.as_array();
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
        return as_object() != other.as_object();
    case kind::array:
        return as_array() != other.as_array();
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

ostream_type& operator <<(ostream_type& stream, const value& val)
{
    switch (val.get_kind())
    {
    case kind::object:
        return stream << val.as_object();
    case kind::array:
        return stream << val.as_array();
    case kind::string:
        return stream_escaped_string(stream, val.as_string());
    case kind::integer:
        return stream << val.as_integer();
    case kind::decimal:
        return stream << val.as_decimal();
    case kind::boolean:
        return stream << (val.as_boolean() ? "true" : "false");
    case kind::null:
    default:
        return stream << "null";
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// object_view                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct object_view_converter;

template <>
struct object_view_converter<object_view::value_type>
{
    union impl
    {
        char* storage;
        object_impl::map_type::iterator* iter;
    };
    
    union const_impl
    {
        const char* storage;
        const object_impl::map_type::iterator* iter;
    };
};

template <>
struct object_view_converter<const object_view::value_type>
{
    union impl
    {
        char* storage;
        object_impl::map_type::const_iterator* iter;
    };
    
    union const_impl
    {
        const char* storage;
        const object_impl::map_type::const_iterator* iter;
    };
};

#define JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(return_, ...)                                                    \
    template return_ object_view::basic_iterator<object_view::value_type>::__VA_ARGS__;                                \
    template return_ object_view::basic_iterator<const object_view::value_type>::__VA_ARGS__;

template <typename T>
object_view::basic_iterator<T>::basic_iterator()
{
    memset(_storage, 0, sizeof _storage);
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(, basic_iterator())

template <typename T>
template <typename U>
object_view::basic_iterator<T>::basic_iterator(const U& source)
{
    static_assert(sizeof(U) == sizeof(_storage), "Input must be the same size of storage");
    
    memcpy(_storage, &source, sizeof _storage);
}

template <typename T>
void object_view::basic_iterator<T>::increment()
{
    typedef typename object_view_converter<T>::impl converter_union;
    converter_union convert;
    convert.storage = _storage;
    ++(*convert.iter);
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(void, increment())

template <typename T>
void object_view::basic_iterator<T>::decrement()
{
    typedef typename object_view_converter<T>::impl converter_union;
    converter_union convert;
    convert.storage = _storage;
    --(*convert.iter);
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(void, decrement())

template <typename T>
T& object_view::basic_iterator<T>::current() const
{
    typedef typename object_view_converter<T>::const_impl converter_union;
    converter_union convert;
    convert.storage = _storage;
    return **convert.iter;
}
template object_view::value_type& object_view::basic_iterator<object_view::value_type>::current() const;
template const object_view::value_type& object_view::basic_iterator<const object_view::value_type>::current() const;

template <typename T>
bool object_view::basic_iterator<T>::equals(const char* other_storage) const
{
    typedef typename object_view_converter<T>::const_impl converter_union;
    converter_union self_convert;
    self_convert.storage = _storage;
    converter_union other_convert;
    other_convert.storage = other_storage;
    
    return *self_convert.iter == *other_convert.iter;
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(bool, equals(const char*) const)

template <typename T>
void object_view::basic_iterator<T>::copy_from(const char* other_storage)
{
    typedef typename object_view_converter<T>::impl       converter_union;
    typedef typename object_view_converter<T>::const_impl const_converter_union;
    converter_union self_convert;
    self_convert.storage = _storage;
    const_converter_union other_convert;
    other_convert.storage = other_storage;
    
    *self_convert.iter = *other_convert.iter;
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(void, copy_from(const char*))

template <typename T>
template <typename U>
object_view::basic_iterator<T>::basic_iterator(const basic_iterator<U>& source,
                                               typename std::enable_if<std::is_convertible<U*, T*>::value>::type*
                                              )
{
    typedef typename object_view_converter<T>::impl       converter_union;
    typedef typename object_view_converter<U>::const_impl const_converter_union;
    converter_union self_convert;
    self_convert.storage = _storage;
    const_converter_union other_convert;
    other_convert.storage = source._storage;
    
    *self_convert.iter = *other_convert.iter;
}
template object_view::basic_iterator<const object_view::value_type>
                    ::basic_iterator<object_view::value_type>(const basic_iterator<object_view::value_type>&, void*);

object_view::object_view(object_impl* source) :
        _source(source)
{ }

object_view::size_type object_view::size() const
{
    return _source->_values.size();
}

object_view::iterator object_view::begin()
{
    return iterator(_source->_values.begin());
}

object_view::const_iterator object_view::begin() const
{
    return const_iterator(_source->_values.begin());
}

object_view::iterator object_view::end()
{
    return iterator(_source->_values.end());
}

object_view::const_iterator object_view::end() const
{
    return const_iterator(_source->_values.end());
}

value& object_view::operator[](const string_type& key)
{
    return _source->_values[key];
}

const value& object_view::operator[](const string_type& key) const
{
    return _source->_values[key];
}

object_view::iterator object_view::find(const string_type& key)
{
    return iterator(_source->_values.find(key));
}

object_view::const_iterator object_view::find(const string_type& key) const
{
    return const_iterator(_source->_values.find(key));
}

void object_view::insert(const value_type& pair)
{
    _source->_values.insert(pair);
}

bool object_view::operator ==(const object_view& other) const
{
    if (this == &other)
        return true;
    if (_source->_values.size() != other._source->_values.size())
        return false;
    
    typedef object_impl::map_type::const_iterator const_iterator;
    const_iterator self_iter  = _source->_values.begin();
    const_iterator other_iter = other._source->_values.begin();
    
    for (const const_iterator self_end = _source->_values.end();
         self_iter != self_end;
         ++self_iter, ++other_iter
        )
    {
        if (self_iter->first != other_iter->first)
            return false;
        if (self_iter->second != other_iter->second)
            return false;
    }
    
    return true;
}

bool object_view::operator !=(const object_view& other) const
{
    return !operator==(other);
}

static void stream_single(ostream_type& stream, const object_impl::map_type::value_type& pair)
{
    stream_escaped_string(stream, pair.first)
        << ":"
        << pair.second;
}
    
ostream_type& operator <<(ostream_type& stream, const object_view& view)
{
    typedef object_impl::map_type::const_iterator const_iterator;
    typedef object_impl::map_type::value_type     value_type;
    
    const_iterator iter = view._source->_values.begin();
    const_iterator end  = view._source->_values.end();
    
    stream << "{";
    
    if (iter != end)
    {
        stream_single(stream, *iter);
        ++iter;
    }
    
    for ( ; iter != end; ++iter)
    {
        stream << ", ";
        stream_single(stream, *iter);
    }
    
    stream << "}";
    
    return stream;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// array_view                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

array_view::array_view(array_impl* source) :
        _source(source)
{ }

array_view::size_type array_view::size() const
{
    return _source->_values.size();
}

bool array_view::empty() const
{
    return _source->_values.empty();
}

array_view::iterator array_view::begin()
{
    return iterator(this, 0);
}

array_view::const_iterator array_view::begin() const
{
    return const_iterator(this, 0);
}

array_view::iterator array_view::end()
{
    return iterator(this, size());
}

array_view::const_iterator array_view::end() const
{
    return const_iterator(this, size());
}

value& array_view::operator[](size_type idx)
{
    return _source->_values[idx];
}

const value& array_view::operator[](size_type idx) const
{
    return _source->_values[idx];
}

void array_view::push_back(const value& val)
{
    _source->_values.push_back(val);
}

void array_view::pop_back()
{
    _source->_values.pop_back();
}

void array_view::push_front(const value& val)
{
    _source->_values.push_front(val);
}

void array_view::pop_front()
{
    _source->_values.pop_front();
}

void array_view::clear()
{
    _source->_values.clear();
}

void array_view::resize(size_type count, const value& val)
{
    _source->_values.resize(count, val);
}

bool array_view::operator ==(const array_view& other) const
{
    if (this == &other)
        return true;
    if (size() != other.size())
        return false;
    
    typedef array_impl::array_type::const_iterator const_iterator;
    
    const_iterator self_iter  = _source->_values.begin();
    const_iterator other_iter = other._source->_values.begin();
    
    for (const const_iterator self_end = _source->_values.end();
         self_iter != self_end;
         ++self_iter, ++other_iter
        )
    {
        if (*self_iter != *other_iter)
            return false;
    }
    
    return true;
}

bool array_view::operator !=(const array_view& other) const
{
    return !operator==(other);
}

ostream_type& operator <<(ostream_type& stream, const array_view& view)
{
    typedef array_impl::array_type::const_iterator const_iterator;
    
    const_iterator iter = view._source->_values.begin();
    const_iterator end  = view._source->_values.end();
    
    stream << "[";
    
    if (iter != end)
    {
        stream << *iter;
        ++iter;
    }
    
    for ( ; iter != end; ++iter)
    {
        stream << ", " << *iter;
    }
    
    stream << "]";
    
    return stream;
}

}
