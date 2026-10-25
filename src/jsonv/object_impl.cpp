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
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

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
    x._data.object = nullptr;
    x._kind = jsonv::kind::object;
    return x;
}

value object(std::initializer_list<std::pair<std::string, value>> source)
{
    value x = object();
    x.insert(source);
    return x;
}

value object(std::initializer_list<std::pair<std::wstring, value>> source)
{
    value x = object();
    x.insert(source);
    return x;
}

namespace
{

using detail::object_impl;
using member_type = object_impl::member_type;

/// Put a member with \a key and \a val at \a pos, unless the member there already has \a key.
///
/// \returns The position of the member with \a key, and whether it was put there.
template <typename TKey>
std::pair<member_type*, bool> place(object_impl*& impl, member_type* pos, TKey&& key, value&& val)
{
    if (pos != object_impl::end(impl) && pos->first == key)
        return { pos, false };
    else
        return { object_impl::emplace(impl, object_impl::index_of(impl, pos), std::forward<TKey>(key), std::move(val)),
                 true
               };
}

}

value::object_iterator value::begin_object()
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(object_impl::data(_data.object));
}

value::const_object_iterator value::begin_object() const
{
    check_type(jsonv::kind::object, kind());
    return const_object_iterator(object_impl::data(_data.object));
}

value::object_iterator value::end_object()
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(object_impl::end(_data.object));
}

value::const_object_iterator value::end_object() const
{
    check_type(jsonv::kind::object, kind());
    return const_object_iterator(object_impl::end(std::as_const(_data.object)));
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
    return place(_data.object, object_impl::lower_bound(_data.object, key), key, value()).first->second;
}

value& value::operator[](std::string&& key)
{
    check_type(jsonv::kind::object, kind());
    auto pos = object_impl::lower_bound(_data.object, key);
    return place(_data.object, pos, std::move(key), value()).first->second;
}

value& value::operator[](const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return operator[](detail::convert_to_narrow(key));
}

value& value::at(const std::string& key)
{
    check_type(jsonv::kind::object, kind());
    auto pos = object_impl::find(_data.object, key);
    if (pos == object_impl::end(_data.object))
        throw std::out_of_range("object has no key \"" + key + "\"");
    return pos->second;
}

const value& value::at(const std::string& key) const
{
    check_type(jsonv::kind::object, kind());
    const object_impl* impl = _data.object;
    auto               pos  = object_impl::find(impl, key);
    if (pos == object_impl::end(impl))
        throw std::out_of_range("object has no key \"" + key + "\"");
    return pos->second;
}

value& value::at(const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return at(detail::convert_to_narrow(key));
}

const value& value::at(const std::wstring& key) const
{
    check_type(jsonv::kind::object, kind());
    return at(detail::convert_to_narrow(key));
}

value::size_type value::count(const std::string& key) const
{
    check_type(jsonv::kind::object, kind());
    const object_impl* impl = _data.object;
    return object_impl::find(impl, key) == object_impl::end(impl) ? 0U : 1U;
}

value::size_type value::count(const std::wstring& key) const
{
    check_type(jsonv::kind::object, kind());
    return count(detail::convert_to_narrow(key));
}

value::object_iterator value::find(const std::string& key)
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(object_impl::find(_data.object, key));
}

value::object_iterator value::find(const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return find(detail::convert_to_narrow(key));
}

value::const_object_iterator value::find(const std::string& key) const
{
    check_type(jsonv::kind::object, kind());
    return const_object_iterator(object_impl::find(std::as_const(_data.object), key));
}

value::const_object_iterator value::find(const std::wstring& key) const
{
    check_type(jsonv::kind::object, kind());
    return find(detail::convert_to_narrow(key));
}

value::object_iterator value::insert(value::const_object_iterator hint, std::pair<std::string, value> pair)
{
    check_type(jsonv::kind::object, kind());
    auto pos = object_impl::position_for(_data.object, hint._member, pair.first);
    return object_iterator(place(_data.object, pos, std::move(pair.first), std::move(pair.second)).first);
}

value::object_iterator value::insert(value::const_object_iterator hint, std::pair<std::wstring, value> pair)
{
    check_type(jsonv::kind::object, kind());
    return insert(hint, { detail::convert_to_narrow(pair.first), std::move(pair.second) });
}

std::pair<value::object_iterator, bool> value::insert(std::pair<std::string, value> pair)
{
    check_type(jsonv::kind::object, kind());
    auto pos = object_impl::lower_bound(_data.object, pair.first);
    auto ret = place(_data.object, pos, std::move(pair.first), std::move(pair.second));
    return { object_iterator(ret.first), ret.second };
}

std::pair<value::object_iterator, bool> value::insert(std::pair<std::wstring, value> pair)
{
    check_type(jsonv::kind::object, kind());
    return insert({ detail::convert_to_narrow(pair.first), std::move(pair.second) });
}

void value::insert(std::initializer_list<std::pair<std::string, value>> items)
{
    check_type(jsonv::kind::object, kind());
    for (const auto& pair : items)
    {
        // Looked up before anything is copied, so a key which is already present costs no copy of the value.
        auto pos = object_impl::lower_bound(_data.object, pair.first);
        if (pos == object_impl::end(_data.object) || pos->first != pair.first)
            object_impl::emplace(_data.object, object_impl::index_of(_data.object, pos), pair.first, pair.second);
    }
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

    // The handle gives its element up only when the insertion happens, so on a collision it keeps ownership and the
    // returned position's key is comparable to handle.key().
    auto pos = object_impl::lower_bound(_data.object, handle.key());
    auto ret = place(_data.object, pos, std::move(handle.key()), std::move(handle.mapped()));
    if (ret.second)
        handle._has_value = false;

    return { const_object_iterator(ret.first), ret.second };
}

value::object_iterator value::insert(const_object_iterator hint, object_node_handle&& handle)
{
    check_type(jsonv::kind::object, kind());
    if (handle.empty())
        return end_object();

    // As with the overload without a hint, the handle keeps ownership of its element when the insertion does not
    // happen.
    auto pos = object_impl::position_for(_data.object, hint._member, handle.key());
    auto ret = place(_data.object, pos, std::move(handle.key()), std::move(handle.mapped()));
    if (ret.second)
        handle._has_value = false;

    return object_iterator(ret.first);
}

std::pair<value::object_iterator, bool> value::emplace(std::string key, value val)
{
    check_type(jsonv::kind::object, kind());
    auto pos = object_impl::lower_bound(_data.object, key);
    auto ret = place(_data.object, pos, std::move(key), std::move(val));
    return { object_iterator(ret.first), ret.second };
}

std::pair<value::object_iterator, bool> value::emplace(const std::wstring& key, value val)
{
    check_type(jsonv::kind::object, kind());
    return emplace(detail::convert_to_narrow(key), std::move(val));
}

// `val` is moved into the object. clang-tidy 19 does not follow the move through `place`'s forwarding parameter.
// NOLINTNEXTLINE(performance-unnecessary-value-param)
std::pair<value::object_iterator, bool> value::try_emplace(const std::string& key, value val)
{
    check_type(jsonv::kind::object, kind());
    auto ret = place(_data.object, object_impl::lower_bound(_data.object, key), key, std::move(val));
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
    auto pos = object_impl::lower_bound(_data.object, key);
    if (pos != object_impl::end(_data.object) && pos->first == key)
    {
        pos->second = std::move(val);
        return { object_iterator(pos), false };
    }
    else
    {
        pos = object_impl::emplace(_data.object, object_impl::index_of(_data.object, pos), key, std::move(val));
        return { object_iterator(pos), true };
    }
}

std::pair<value::object_iterator, bool> value::insert_or_assign(const std::wstring& key, value val)
{
    check_type(jsonv::kind::object, kind());
    return insert_or_assign(detail::convert_to_narrow(key), std::move(val));
}

value::size_type value::erase(const std::string& key)
{
    check_type(jsonv::kind::object, kind());
    auto pos = object_impl::find(_data.object, key);
    if (pos == object_impl::end(_data.object))
        return 0U;

    auto index = object_impl::index_of(_data.object, pos);
    object_impl::erase(_data.object, index, index + 1U);
    return 1U;
}

value::size_type value::erase(const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return erase(detail::convert_to_narrow(key));
}

value::object_iterator value::erase(const_object_iterator position)
{
    check_type(jsonv::kind::object, kind());
    auto index = object_impl::index_of(_data.object, position._member);
    object_impl::erase(_data.object, index, index + 1U);
    return object_iterator(object_impl::data(_data.object) + index);
}

value::object_iterator value::erase(const_object_iterator first, const_object_iterator last)
{
    check_type(jsonv::kind::object, kind());
    auto first_index = object_impl::index_of(_data.object, first._member);
    object_impl::erase(_data.object, first_index, object_impl::index_of(_data.object, last._member));
    return object_iterator(object_impl::data(_data.object) + first_index);
}

object_node_handle value::extract(const_object_iterator position)
{
    check_type(jsonv::kind::object, kind());

    // Being able to rewrite the key of an extracted element is the whole point of a node handle, so the key moves out
    // of the object rather than being copied.
    auto         index  = object_impl::index_of(_data.object, position._member);
    member_type& member = object_impl::data(_data.object)[index];
    object_node_handle out(object_node_handle::purposeful_construction(),
                           std::move(member.first),
                           std::move(member.second)
                          );
    object_impl::erase(_data.object, index, index + 1U);
    return out;
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// object_impl                                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

object_impl::member_type* object_impl::position_for(object_impl* self, const member_type* hint, std::string_view key)
{
    auto pos = data(self) + index_of(self, hint);
    if (  (pos == data(self)  || std::string_view(std::prev(pos)->first) < key)
       && (pos == end(self)   || key < std::string_view(pos->first))
       )
        return pos;
    else
        return lower_bound(self, key);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// object_builder                                                                                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::pair<object_builder::member_type*, bool> object_builder::find_indexed(const index_type& index,
                                                                           std::string_view  key
                                                                          )
{
    auto found = index.find(key);
    if (found == index.end())
        return { object_impl::end(*_target), false };
    else
        return { object_impl::data(*_target) + *found, true };
}

void object_builder::add_indexed(member_type* where, std::string&& key, value&& val)
{
    // With an index, members are appended; without one, this is the member which goes past `sorted_insert_limit`, and
    // it still goes where it belongs. Either way, `emplace` can not fail once it has moved from its arguments, and if
    // indexing the member then throws, it is taken back out.
    auto index = _index ? object_impl::size(*_target) : object_impl::index_of(*_target, where);
    where = object_impl::emplace(*_target, index, std::move(key), std::move(val));

    try
    {
        if (_index)
        {
            _index->insert(index);
        }
        else
        {
            // The members are in key order so far, so each goes at the end of the index.
            index_type built(key_of_position{ _target });
            for (std::size_t pos = 0U; pos < object_impl::size(*_target); ++pos)
                built.insert(built.end(), pos);
            _index = std::move(built);
        }
    }
    catch (...)
    {
        key = std::move(where->first);
        val = std::move(where->second);
        object_impl::erase(*_target, index, index + 1U);
        throw;
    }
}

void object_builder::sort_indexed()
{
    // The first `sorted_insert_limit` members were inserted in order and the rest appended. The index refers to them by
    // position, which sorting changes. Every key is distinct, so the order the sort leaves equal keys in does not come
    // up.
    _index.reset();
    std::sort(object_impl::data(*_target),
              object_impl::end(*_target),
              [] (const member_type& a, const member_type& b)
              {
                  return a.first < b.first;
              }
             );
}

}

}
