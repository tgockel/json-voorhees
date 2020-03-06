/// \file
/// Implementation of \c jsonv::value member functions related to objects.
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "object_impl.hpp"

#include <jsonv/char_convert.hpp>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// object_node_handle                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

object_node_handle::object_node_handle(purposeful_construction, key_type key, mapped_type value) noexcept :
        _has_value(true),
        _key(std::move(key)),
        _value(std::move(value))
{ }

object_node_handle::object_node_handle(object_node_handle&& src) noexcept :
        _has_value(src._has_value),
        _key(std::move(src._key)),
        _value(std::move(src._value))
{
    src._has_value = false;
}

object_node_handle& object_node_handle::operator=(object_node_handle&& src) noexcept
{
    _has_value = src._has_value;
    _key       = std::move(src._key);
    _value     = std::move(src._value);
    return *this;
}

object_node_handle::~object_node_handle() noexcept
{ }

object_node_handle::key_type& object_node_handle::key() const
{
    if (empty())
        throw std::invalid_argument("object_node_handle is empty");
    return _key;
}

object_node_handle::mapped_type& object_node_handle::mapped() const
{
    if (empty())
        throw std::invalid_argument("object_node_handle is empty");
    return _value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// object                                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

value object()
{
    value x;
    x._data.object = new detail::object_impl;
    x._kind = jsonv::kind::object;
    return x;
}

value object(std::initializer_list<std::pair<std::string, value>> source)
{
    value x = object();
    x.insert(std::move(source));
    return x;
}

value object(std::initializer_list<std::pair<std::wstring, value>> source)
{
    value x = object();
    x.insert(std::move(source));
    return x;
}

value::object_iterator value::begin_object()
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(_data.object->_values.begin());
}

value::const_object_iterator value::begin_object() const
{
    check_type(jsonv::kind::object, kind());
    return const_object_iterator(_data.object->_values.begin());
}

value::object_iterator value::end_object()
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(_data.object->_values.end());
}

value::const_object_iterator value::end_object() const
{
    check_type(jsonv::kind::object, kind());
    return const_object_iterator(_data.object->_values.end());
}

value::object_view value::as_object() &
{
    return object_view(begin_object(), end_object());
}

value::const_object_view value::as_object() const &
{
    return const_object_view(begin_object(), end_object());
}

value::owning_object_view value::as_object() &&
{
    check_type(jsonv::kind::object, kind());
    return owning_object_view(std::move(*this),
                              [] (value& x) { return x.begin_object(); },
                              [] (value& x) { return x.end_object(); }
                             );
}

value& value::operator[](const std::string& key)
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values[key];
}

value& value::operator[](std::string&& key)
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values[std::move(key)];
}

value& value::operator[](const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values[detail::convert_to_narrow(key)];
}

value& value::at(const std::string& key)
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values.at(key);
}

const value& value::at(const std::string& key) const
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values.at(key);
}

value& value::at(const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values.at(detail::convert_to_narrow(key));
}

const value& value::at(const std::wstring& key) const
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values.at(detail::convert_to_narrow(key));
}

value::size_type value::count(const std::string& key) const
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values.count(key);
}

value::size_type value::count(const std::wstring& key) const
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values.count(detail::convert_to_narrow(key));
}

value::object_iterator value::find(const std::string& key)
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(_data.object->_values.find(key));
}

value::object_iterator value::find(const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(_data.object->_values.find(detail::convert_to_narrow(key)));
}

value::const_object_iterator value::find(const std::string& key) const
{
    check_type(jsonv::kind::object, kind());
    return const_object_iterator(_data.object->_values.find(key));
}

value::const_object_iterator value::find(const std::wstring& key) const
{
    check_type(jsonv::kind::object, kind());
    return const_object_iterator(_data.object->_values.find(detail::convert_to_narrow(key)));
}

value::object_iterator value::insert(value::const_object_iterator hint, std::pair<std::string, value> pair)
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(_data.object->_values.insert(hint._impl, std::move(pair)));
}

value::object_iterator value::insert(value::const_object_iterator hint, std::pair<std::wstring, value> pair)
{
    check_type(jsonv::kind::object, kind());
    return insert(hint, { detail::convert_to_narrow(pair.first), std::move(pair.second) });
}

std::pair<value::object_iterator, bool> value::insert(std::pair<std::string, value> pair)
{
    check_type(jsonv::kind::object, kind());
    auto ret = _data.object->_values.insert(pair);
    return { object_iterator(ret.first), ret.second };
}

std::pair<value::object_iterator, bool> value::insert(std::pair<std::wstring, value> pair)
{
    check_type(jsonv::kind::object, kind());
    auto ret = _data.object->_values.insert({ detail::convert_to_narrow(pair.first), std::move(pair.second) });
    return { object_iterator(ret.first), ret.second };
}

void value::insert(std::initializer_list<std::pair<std::string, value>> items)
{
    check_type(jsonv::kind::object, kind());
    for (auto& pair : items)
         _data.object->_values.insert(std::move(pair));
}

void value::insert(std::initializer_list<std::pair<std::wstring, value>> items)
{
    check_type(jsonv::kind::object, kind());
    for (auto& pair : items)
         insert(std::move(pair));
}

value::object_insert_return_type value::insert(object_node_handle&& handle)
{
    check_type(jsonv::kind::object, kind());
    if (handle.empty())
        return { end_object(), false };

    auto insert_rc = _data.object->_values.insert({ std::move(handle.key()), std::move(handle.mapped()) });
    return { const_object_iterator(insert_rc.first), insert_rc.second };
}

value::object_iterator value::insert(const_object_iterator, object_node_handle&& handle)
{
    check_type(jsonv::kind::object, kind());
    if (handle.empty())
        return end_object();

    auto place = _data.object->_values.find(handle.key());
    if (place == _data.object->_values.end())
    {
        return insert({ std::move(handle.key()), std::move(handle.mapped()) }).first;
    }
    else
    {
        return object_iterator(place);
    }
}

value::size_type value::erase(const std::string& key)
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values.erase(key);
}

value::size_type value::erase(const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return _data.object->_values.erase(detail::convert_to_narrow(key));
}

value::object_iterator value::erase(const_object_iterator position)
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(_data.object->_values.erase(position._impl));
}

value::object_iterator value::erase(const_object_iterator first, const_object_iterator last)
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(_data.object->_values.erase(first._impl, last._impl));
}

template <typename TMap>
class has_extract
{
private:
    template <typename U, U>
    class check
    { };

    template <typename UMap>
    static char f(check<typename UMap::node_handle (UMap::*)(typename UMap::const_iterator), &UMap::extract>*);

    template <typename UMap>
    static long f(...);

public:
    static const bool value = (sizeof(f<TMap>(0)) == sizeof(char));
};

template <typename TMap>
using has_extract_t = std::integral_constant<bool, has_extract<TMap>::value>;

template <typename TMapImpl, typename TIterator, typename FCreate>
object_node_handle extract_impl(TMapImpl& impl, TIterator position, FCreate&& create, std::true_type)
{
    auto handle = impl.extract(position);
    return create(std::move(handle.key()), std::move(handle.mapped()));
}

template <typename TMapImpl, typename TIterator, typename FCreate>
object_node_handle extract_impl(TMapImpl& impl, TIterator position, FCreate&& create, std::false_type)
{
    auto iter = impl.find(position->first);
    auto out  = create(iter->first, std::move(iter->second));
    impl.erase(iter);
    return out;
}

template <typename TMapImpl, typename TIterator, typename FCreate>
object_node_handle extract_impl(TMapImpl& impl, TIterator position, FCreate&& create)
{
    return extract_impl(impl, position, std::forward<FCreate>(create), has_extract_t<TMapImpl>());
}

object_node_handle value::extract(const_object_iterator position)
{
    check_type(jsonv::kind::object, kind());
    return extract_impl(_data.object->_values,
                        position._impl,
                        [] (std::string key, value x)
                        {
                            return object_node_handle(object_node_handle::purposeful_construction(),
                                                      std::move(key),
                                                      std::move(x)
                                                     );
                        }
                       );
}

object_node_handle value::extract(const std::string& key)
{
    const_object_iterator iter = find(key);
    if (iter != end_object())
        return extract(iter);
    else
        return object_node_handle();
}

object_node_handle value::extract(const std::wstring& key)
{
    const_object_iterator iter = find(key);
    if (iter != end_object())
        return extract(iter);
    else
        return object_node_handle();
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

}
}
