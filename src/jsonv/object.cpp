/** \file
 *  Implementation of \c jsonv::value member functions related to objects.
 *  
 *  Copyright (c) 2012-2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/object.hpp>

#include <cstring>

using namespace jsonv::detail;

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// value::basic_object_iterator<T>                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct object_iter_converter;

template <>
struct object_iter_converter<value::object_value_type>
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
struct object_iter_converter<const value::object_value_type>
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
    template return_ value::basic_object_iterator<value::object_value_type>::__VA_ARGS__;                              \
    template return_ value::basic_object_iterator<const value::object_value_type>::__VA_ARGS__;                        \

template <typename T>
value::basic_object_iterator<T>::basic_object_iterator()
{
    memset(_storage, 0, sizeof _storage);
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(, basic_object_iterator())

// private
template <typename T>
template <typename U>
value::basic_object_iterator<T>::basic_object_iterator(const U& source)
{
    static_assert(sizeof(U) == sizeof(_storage), "Input must be the same size of storage");
    
    memcpy(_storage, &source, sizeof _storage);
}

template <typename T>
void value::basic_object_iterator<T>::increment()
{
    typedef typename object_iter_converter<T>::impl converter_union;
    converter_union convert;
    convert.storage = _storage;
    ++(*convert.iter);
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(void, increment())

template <typename T>
void value::basic_object_iterator<T>::decrement()
{
    typedef typename object_iter_converter<T>::impl converter_union;
    converter_union convert;
    convert.storage = _storage;
    --(*convert.iter);
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(void, decrement())

template <typename T>
T& value::basic_object_iterator<T>::current() const
{
    typedef typename object_iter_converter<T>::const_impl converter_union;
    converter_union convert;
    convert.storage = _storage;
    return **convert.iter;
}
template value::object_value_type& value::basic_object_iterator<value::object_value_type>::current() const;
template const value::object_value_type& value::basic_object_iterator<const value::object_value_type>::current() const;

template <typename T>
bool value::basic_object_iterator<T>::equals(const char* other_storage) const
{
    typedef typename object_iter_converter<T>::const_impl converter_union;
    converter_union self_convert;
    self_convert.storage = _storage;
    converter_union other_convert;
    other_convert.storage = other_storage;
    
    return *self_convert.iter == *other_convert.iter;
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(bool, equals(const char*) const)

template <typename T>
void value::basic_object_iterator<T>::copy_from(const char* other_storage)
{
    typedef typename object_iter_converter<T>::impl       converter_union;
    typedef typename object_iter_converter<T>::const_impl const_converter_union;
    converter_union self_convert;
    self_convert.storage = _storage;
    const_converter_union other_convert;
    other_convert.storage = other_storage;
    
    *self_convert.iter = *other_convert.iter;
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(void, copy_from(const char*))

template <typename T>
template <typename U>
value::basic_object_iterator<T>::basic_object_iterator(const basic_object_iterator<U>& source,
                                                       typename std::enable_if<std::is_convertible<U*, T*>::value>::type*
                                                      )
{
    typedef typename object_iter_converter<T>::impl       converter_union;
    typedef typename object_iter_converter<U>::const_impl const_converter_union;
    converter_union self_convert;
    self_convert.storage = _storage;
    const_converter_union other_convert;
    other_convert.storage = source._storage;
    
    *self_convert.iter = *other_convert.iter;
}
template value::basic_object_iterator<const value::object_value_type>
              ::basic_object_iterator(const basic_object_iterator<value::object_value_type>&, void*);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// object                                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define OBJ _data.object->_values

value object()
{
    value x;
    x._data.object = new detail::object_impl;
    x._kind = kind::object;
    return x;
}

value object(std::initializer_list<std::pair<const std::string, value>> source)
{
    value x = object();
    x.insert(std::move(source));
    return x;
}

value::object_iterator value::begin_object()
{
    check_type(kind::object, get_kind());
    return object_iterator(OBJ.begin());
}

value::const_object_iterator value::begin_object() const
{
    check_type(kind::object, get_kind());
    return const_object_iterator(OBJ.begin());
}

value::object_iterator value::end_object()
{
    check_type(kind::object, get_kind());
    return object_iterator(OBJ.end());
}

value::const_object_iterator value::end_object() const
{
    check_type(kind::object, get_kind());
    return const_object_iterator(OBJ.end());
}

value& value::operator[](const std::string& key)
{
    check_type(kind::object, get_kind());
    return OBJ[key];
}

const value& value::operator[](const std::string& key) const
{
    check_type(kind::object, get_kind());
    return OBJ[key];
}

value& value::operator[](const char* key)
{
    check_type(kind::object, get_kind());
    return OBJ[key];
}

const value& value::operator[](const char* key) const
{
    check_type(kind::object, get_kind());
    return OBJ[key];
}

value& value::at(const std::string& key)
{
    check_type(kind::object, get_kind());
    return OBJ.at(key);
}

const value& value::at(const std::string& key) const
{
    check_type(kind::object, get_kind());
    return OBJ.at(key);
}

value& value::at(const char* key)
{
    check_type(kind::object, get_kind());
    return OBJ.at(key);
}

const value& value::at(const char* key) const
{
    check_type(kind::object, get_kind());
    return OBJ.at(key);
}

value::size_type value::count(const std::string& key) const
{
    check_type(kind::object, get_kind());
    return OBJ.count(key);
}

value::size_type value::count(const char* key) const
{
    check_type(kind::object, get_kind());
    return OBJ.count(key);
}

value::object_iterator value::find(const std::string& key)
{
    check_type(kind::object, get_kind());
    return object_iterator(OBJ.find(key));
}

value::const_object_iterator value::find(const std::string& key) const
{
    check_type(kind::object, get_kind());
    return const_object_iterator(OBJ.find(key));
}

value::object_iterator value::find(const char* key)
{
    check_type(kind::object, get_kind());
    return object_iterator(OBJ.find(key));
}

value::const_object_iterator value::find(const char* key) const
{
    check_type(kind::object, get_kind());
    return const_object_iterator(OBJ.find(key));
}

value::object_iterator value::insert(value::const_object_iterator hint, const std::pair<std::string, value>& pair)
{
    check_type(kind::object, get_kind());
    typedef typename object_iter_converter<const value::object_value_type>::const_impl const_converter_union;
    const_converter_union hint_convert;
    hint_convert.storage = hint._storage;
    return object_iterator(OBJ.insert(*hint_convert.iter, pair));
}

value::object_iterator value::insert(value::const_object_iterator hint, const std::pair<const char*, value>& pair)
{
    check_type(kind::object, get_kind());
    typedef typename object_iter_converter<const value::object_value_type>::const_impl const_converter_union;
    const_converter_union hint_convert;
    hint_convert.storage = hint._storage;
    return object_iterator(OBJ.insert(*hint_convert.iter, pair));
}

std::pair<value::object_iterator, bool> value::insert(const std::pair<std::string, value>& pair)
{
    check_type(kind::object, get_kind());
    auto ret = OBJ.insert(pair);
    return { object_iterator(ret.first), ret.second };
}

std::pair<value::object_iterator, bool> value::insert(const std::pair<const char*, value>& pair)
{
    check_type(kind::object, get_kind());
    auto ret = OBJ.insert(pair);
    return { object_iterator(ret.first), ret.second };
}

void value::insert(std::initializer_list<object_value_type> items)
{
    check_type(kind::object, get_kind());
    OBJ.insert(items);
}

void value::insert(std::initializer_list<std::pair<const char*, value>> items)
{
    check_type(kind::object, get_kind());
    OBJ.insert(items.begin(), items.end());
}

value::size_type value::erase(const std::string& key)
{
    check_type(kind::object, get_kind());
    return OBJ.erase(key);
}

value::size_type value::erase(const char* key)
{
    check_type(kind::object, get_kind());
    return OBJ.erase(key);
}

value::object_iterator value::erase(const_object_iterator position)
{
    check_type(kind::object, get_kind());
    typedef typename object_iter_converter<const value::object_value_type>::const_impl const_converter_union;
    const_converter_union pos_convert;
    pos_convert.storage = position._storage;
    return object_iterator(OBJ.erase(*pos_convert.iter));
}

value::object_iterator value::erase(const_object_iterator first, const_object_iterator last)
{
    check_type(kind::object, get_kind());
    typedef typename object_iter_converter<const value::object_value_type>::const_impl const_converter_union;
    const_converter_union first_convert;
    first_convert.storage = first._storage;
    const_converter_union last_convert;
    last_convert.storage = last._storage;
    return object_iterator(OBJ.erase(*first_convert.iter, *last_convert.iter));
}

namespace detail
{

bool object_impl::empty() const
{
    return _values.empty();
}

value::size_type object_impl::size() const
{
    return _values.size();
}

int object_impl::compare(const object_impl& other) const
{
    auto self_iter = _values.begin();
    auto othr_iter = other._values.begin();
    for (; self_iter != _values.end() && othr_iter != other._values.end(); ++self_iter, ++othr_iter)
    {
        if (int cmp = self_iter->first.compare(othr_iter->first))
            return cmp;
        if (int cmp = self_iter->second.compare(othr_iter->second))
            return cmp;
    }
    return self_iter == _values.end() ? othr_iter == other._values.end() ? 0 : -1 : 1;
}

bool object_impl::operator==(const object_impl& other) const
{
    if (this == &other)
        return true;
    if (_values.size() != other._values.size())
        return false;
    
    typedef object_impl::map_type::const_iterator const_iterator;
    const_iterator self_iter  = _values.begin();
    const_iterator other_iter = other._values.begin();
    
    for (const const_iterator self_end = _values.end();
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

bool object_impl::operator!=(const object_impl& other) const
{
    return !operator==(other);
}

static void stream_single(std::ostream& stream, const object_impl::map_type::value_type& pair)
{
    stream_escaped_string(stream, pair.first)
        << ":"
        << pair.second;
}

std::ostream& operator<<(std::ostream& stream, const object_impl& view)
{
    typedef object_impl::map_type::const_iterator const_iterator;
    
    const_iterator iter = view._values.begin();
    const_iterator end  = view._values.end();
    
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

}
}
