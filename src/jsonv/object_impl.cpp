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
    // The self-assignment guard is not just an optimization: self-move of an std::string leaves it valid but with an
    // unspecified value, so without the check `handle = std::move(handle)` could silently discard the key.
    if (this != &src)
    {
        _has_value     = src._has_value;
        _key           = std::move(src._key);
        _value         = std::move(src._value);
        src._has_value = false;
    }
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
    auto ret = _data.object->_values.try_emplace(std::move(pair.first), std::move(pair.second));
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
         insert(pair);
}

value::object_insert_return_type value::insert(object_node_handle&& handle)
{
    check_type(jsonv::kind::object, kind());
    if (handle.empty())
        return { end_object(), false };

    // try_emplace leaves its arguments alone when the key is already present, so the handle keeps ownership of its
    // element when the insertion does not happen -- which is what makes the returned position's key comparable to
    // handle.key(). map::insert would have moved the contents out before it ever looked the key up.
    auto insert_rc = _data.object->_values.try_emplace(std::move(handle.key()), std::move(handle.mapped()));
    if (insert_rc.second)
        handle._has_value = false;

    return { const_object_iterator(insert_rc.first), insert_rc.second };
}

value::object_iterator value::insert(const_object_iterator hint, object_node_handle&& handle)
{
    check_type(jsonv::kind::object, kind());
    if (handle.empty())
        return end_object();

    // try_emplace leaves its arguments alone when the key is already present, which is exactly the contract this
    // overload documents -- the handle keeps ownership of the element if the insertion does not happen. The hinted
    // overload returns a bare iterator, so the size is what says whether the insertion happened.
    auto& values      = _data.object->_values;
    auto  size_before = values.size();
    auto  pos         = values.try_emplace(hint._impl, std::move(handle.key()), std::move(handle.mapped()));
    if (values.size() != size_before)
        handle._has_value = false;

    return object_iterator(pos);
}

std::pair<value::object_iterator, bool> value::emplace(std::string key, value val)
{
    check_type(jsonv::kind::object, kind());
    auto ret = _data.object->_values.emplace(std::move(key), std::move(val));
    return { object_iterator(ret.first), ret.second };
}

std::pair<value::object_iterator, bool> value::emplace(const std::wstring& key, value val)
{
    check_type(jsonv::kind::object, kind());
    return emplace(detail::convert_to_narrow(key), std::move(val));
}

std::pair<value::object_iterator, bool> value::try_emplace(const std::string& key, value val)
{
    check_type(jsonv::kind::object, kind());
    auto ret = _data.object->_values.try_emplace(key, std::move(val));
    return { object_iterator(ret.first), ret.second };
}

std::pair<value::object_iterator, bool> value::try_emplace(const std::wstring& key, value val)
{
    check_type(jsonv::kind::object, kind());
    return try_emplace(detail::convert_to_narrow(key), std::move(val));
}

std::pair<value::object_iterator, bool> value::insert_or_assign(const std::string& key, value val)
{
    check_type(jsonv::kind::object, kind());
    auto ret = _data.object->_values.insert_or_assign(key, std::move(val));
    return { object_iterator(ret.first), ret.second };
}

std::pair<value::object_iterator, bool> value::insert_or_assign(const std::wstring& key, value val)
{
    check_type(jsonv::kind::object, kind());
    return insert_or_assign(detail::convert_to_narrow(key), std::move(val));
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

object_node_handle value::extract(const_object_iterator position)
{
    check_type(jsonv::kind::object, kind());

    // node_type::key() hands back a non-const reference -- being able to rewrite the key of an extracted element is
    // the whole point of a node handle -- so the key moves out of the node instead of being copied.
    auto node = _data.object->_values.extract(position._impl);
    return object_node_handle(object_node_handle::purposeful_construction(),
                              std::move(node.key()),
                              std::move(node.mapped())
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
