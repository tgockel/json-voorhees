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
#include <json-voorhees/object.hpp>

#include "detail.hpp"

#include <cstring>

using namespace jsonv::detail;

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// object                                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define OBJ _data.object->_values

object::object()
{
    _data.object = new detail::object_impl;
    _kind = kind::object;
}

object::object(const object& source) :
        value(source)
{ }

object::object(object&& source) :
        value(std::move(source))
{ }

object& object::operator =(const object& source)
{
    value::operator=(source);
    return *this;
}

object& object::operator =(object&& source)
{
    value::operator=(std::move(source));
    return *this;
}

void object::ensure_object() const
{
    check_type(kind::object, get_kind());
}

object::size_type object::size() const
{
    ensure_object();
    return OBJ.size();
}

object::iterator object::begin()
{
    ensure_object();
    return iterator(OBJ.begin());
}

object::const_iterator object::begin() const
{
    ensure_object();
    return const_iterator(OBJ.begin());
}

object::iterator object::end()
{
    ensure_object();
    return iterator(OBJ.end());
}

object::const_iterator object::end() const
{
    ensure_object();
    return const_iterator(OBJ.end());
}

value& object::operator[](const std::string& key)
{
    ensure_object();
    return OBJ[key];
}

const value& object::operator[](const std::string& key) const
{
    ensure_object();
    return OBJ[key];
}

object::size_type object::count(const key_type& key) const
{
    ensure_object();
    return OBJ.count(key);
}

object::iterator object::find(const std::string& key)
{
    ensure_object();
    return iterator(OBJ.find(key));
}

object::const_iterator object::find(const std::string& key) const
{
    ensure_object();
    return const_iterator(OBJ.find(key));
}

void object::insert(const value_type& pair)
{
    OBJ.insert(pair);
}

object::size_type object::erase(const key_type& key)
{
    ensure_object();
    return OBJ.erase(key);
}

bool object::operator ==(const object& other) const
{
    if (this == &other)
        return true;
    if (size() != other.size())
        return false;
    
    typedef object_impl::map_type::const_iterator const_iterator;
    const_iterator self_iter  = OBJ.begin();
    const_iterator other_iter = other.OBJ.begin();
    
    for (const const_iterator self_end = OBJ.end();
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

bool object::operator !=(const object& other) const
{
    return !operator==(other);
}

static void stream_single(std::ostream& stream, const object_impl::map_type::value_type& pair)
{
    stream_escaped_string(stream, pair.first)
        << ":"
        << pair.second;
}

std::ostream& operator <<(std::ostream& stream, const object& view)
{
    typedef object_impl::map_type::const_iterator const_iterator;
    
    const_iterator iter = view.OBJ.begin();
    const_iterator end  = view.OBJ.end();
    
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
// object::basic_iterator<T>                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct object_iter_converter;

template <>
struct object_iter_converter<object::value_type>
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
struct object_iter_converter<const object::value_type>
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
    template return_ object::basic_iterator<object::value_type>::__VA_ARGS__;                                          \
    template return_ object::basic_iterator<const object::value_type>::__VA_ARGS__;                                    \

template <typename T>
object::basic_iterator<T>::basic_iterator()
{
    memset(_storage, 0, sizeof _storage);
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(, basic_iterator())

// private
template <typename T>
template <typename U>
object::basic_iterator<T>::basic_iterator(const U& source)
{
    static_assert(sizeof(U) == sizeof(_storage), "Input must be the same size of storage");
    
    memcpy(_storage, &source, sizeof _storage);
}

template <typename T>
void object::basic_iterator<T>::increment()
{
    typedef typename object_iter_converter<T>::impl converter_union;
    converter_union convert;
    convert.storage = _storage;
    ++(*convert.iter);
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(void, increment())

template <typename T>
void object::basic_iterator<T>::decrement()
{
    typedef typename object_iter_converter<T>::impl converter_union;
    converter_union convert;
    convert.storage = _storage;
    --(*convert.iter);
}
JSONV_INSTANTIATE_OBJVIEW_BASIC_ITERATOR_FUNC(void, decrement())

template <typename T>
T& object::basic_iterator<T>::current() const
{
    typedef typename object_iter_converter<T>::const_impl converter_union;
    converter_union convert;
    convert.storage = _storage;
    return **convert.iter;
}
template object::value_type& object::basic_iterator<object::value_type>::current() const;
template const object::value_type& object::basic_iterator<const object::value_type>::current() const;

template <typename T>
bool object::basic_iterator<T>::equals(const char* other_storage) const
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
void object::basic_iterator<T>::copy_from(const char* other_storage)
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
object::basic_iterator<T>::basic_iterator(const basic_iterator<U>& source,
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
template object::basic_iterator<const object::value_type>
               ::basic_iterator<object::value_type>(const basic_iterator<object::value_type>&, void*);


object::iterator object::erase(const_iterator position)
{
    ensure_object();
    typedef typename object_iter_converter<const object::value_type>::const_impl const_converter_union;
    const_converter_union pos_convert;
    pos_convert.storage = position._storage;
    return iterator(OBJ.erase(*pos_convert.iter));
}

object::iterator object::erase(const_iterator first, const_iterator last)
{
    ensure_object();
    typedef typename object_iter_converter<const object::value_type>::const_impl const_converter_union;
    const_converter_union first_convert;
    first_convert.storage = first._storage;
    const_converter_union last_convert;
    last_convert.storage = last._storage;
    return iterator(OBJ.erase(*first_convert.iter, *last_convert.iter));
}

}
