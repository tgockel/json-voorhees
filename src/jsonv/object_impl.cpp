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
    x._data.object = new detail::object_impl;
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

/// Put a member with \a key and \a val at \a pos, unless the member there already has \a key.
///
/// \returns The position of the member with \a key, and whether it was put there.
template <typename TKey>
std::pair<detail::object_impl::iterator, bool> place(detail::object_impl&           impl,
                                                     detail::object_impl::iterator pos,
                                                     TKey&&                        key,
                                                     value&&                       val
                                                    )
{
    if (pos != impl._members.end() && pos->first == key)
        return { pos, false };
    else
        return { impl._members.emplace(pos, std::forward<TKey>(key), std::move(val)), true };
}

}

value::object_iterator value::begin_object()
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(_data.object->_members.data());
}

value::const_object_iterator value::begin_object() const
{
    check_type(jsonv::kind::object, kind());
    return const_object_iterator(_data.object->_members.data());
}

value::object_iterator value::end_object()
{
    check_type(jsonv::kind::object, kind());
    return object_iterator(_data.object->_members.data() + _data.object->_members.size());
}

value::const_object_iterator value::end_object() const
{
    check_type(jsonv::kind::object, kind());
    return const_object_iterator(_data.object->_members.data() + _data.object->_members.size());
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
    auto& impl = *_data.object;
    return place(impl, impl.lower_bound(key), key, value()).first->second;
}

value& value::operator[](std::string&& key)
{
    check_type(jsonv::kind::object, kind());
    auto& impl = *_data.object;
    return place(impl, impl.lower_bound(key), std::move(key), value()).first->second;
}

value& value::operator[](const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return operator[](detail::convert_to_narrow(key));
}

value& value::at(const std::string& key)
{
    check_type(jsonv::kind::object, kind());
    auto pos = _data.object->find(key);
    if (pos == _data.object->_members.end())
        throw std::out_of_range("object has no key \"" + key + "\"");
    return pos->second;
}

const value& value::at(const std::string& key) const
{
    check_type(jsonv::kind::object, kind());
    auto pos = std::as_const(*_data.object).find(key);
    if (pos == _data.object->_members.cend())
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
    return std::as_const(*_data.object).find(key) == _data.object->_members.cend() ? 0U : 1U;
}

value::size_type value::count(const std::wstring& key) const
{
    check_type(jsonv::kind::object, kind());
    return count(detail::convert_to_narrow(key));
}

value::object_iterator value::find(const std::string& key)
{
    check_type(jsonv::kind::object, kind());
    auto& impl = *_data.object;
    return object_iterator(impl.address_of(impl.find(key)));
}

value::object_iterator value::find(const std::wstring& key)
{
    check_type(jsonv::kind::object, kind());
    return find(detail::convert_to_narrow(key));
}

value::const_object_iterator value::find(const std::string& key) const
{
    check_type(jsonv::kind::object, kind());
    auto& impl = *_data.object;
    return const_object_iterator(impl.address_of(std::as_const(impl).find(key)));
}

value::const_object_iterator value::find(const std::wstring& key) const
{
    check_type(jsonv::kind::object, kind());
    return find(detail::convert_to_narrow(key));
}

value::object_iterator value::insert(value::const_object_iterator hint, std::pair<std::string, value> pair)
{
    check_type(jsonv::kind::object, kind());
    auto& impl = *_data.object;
    auto  pos  = impl.position_for(impl.position_of(hint._member), pair.first);
    return object_iterator(impl.address_of(place(impl, pos, std::move(pair.first), std::move(pair.second)).first));
}

value::object_iterator value::insert(value::const_object_iterator hint, std::pair<std::wstring, value> pair)
{
    check_type(jsonv::kind::object, kind());
    return insert(hint, { detail::convert_to_narrow(pair.first), std::move(pair.second) });
}

std::pair<value::object_iterator, bool> value::insert(std::pair<std::string, value> pair)
{
    check_type(jsonv::kind::object, kind());
    auto& impl = *_data.object;
    auto  ret  = place(impl, impl.lower_bound(pair.first), std::move(pair.first), std::move(pair.second));
    return { object_iterator(impl.address_of(ret.first)), ret.second };
}

std::pair<value::object_iterator, bool> value::insert(std::pair<std::wstring, value> pair)
{
    check_type(jsonv::kind::object, kind());
    return insert({ detail::convert_to_narrow(pair.first), std::move(pair.second) });
}

void value::insert(std::initializer_list<std::pair<std::string, value>> items)
{
    check_type(jsonv::kind::object, kind());
    auto& impl = *_data.object;
    for (const auto& pair : items)
    {
        // Looked up before anything is copied, so a key which is already present costs no copy of the value.
        auto pos = impl.lower_bound(pair.first);
        if (pos == impl._members.end() || pos->first != pair.first)
            impl._members.emplace(pos, pair.first, pair.second);
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
    auto& impl = *_data.object;
    auto  ret  = place(impl, impl.lower_bound(handle.key()), std::move(handle.key()), std::move(handle.mapped()));
    if (ret.second)
        handle._has_value = false;

    return { const_object_iterator(impl.address_of(ret.first)), ret.second };
}

value::object_iterator value::insert(const_object_iterator hint, object_node_handle&& handle)
{
    check_type(jsonv::kind::object, kind());
    if (handle.empty())
        return end_object();

    // As with the overload without a hint, the handle keeps ownership of its element when the insertion does not
    // happen.
    auto& impl = *_data.object;
    auto  pos  = impl.position_for(impl.position_of(hint._member), handle.key());
    auto  ret  = place(impl, pos, std::move(handle.key()), std::move(handle.mapped()));
    if (ret.second)
        handle._has_value = false;

    return object_iterator(impl.address_of(ret.first));
}

std::pair<value::object_iterator, bool> value::emplace(std::string key, value val)
{
    check_type(jsonv::kind::object, kind());
    auto& impl = *_data.object;
    auto  ret  = place(impl, impl.lower_bound(key), std::move(key), std::move(val));
    return { object_iterator(impl.address_of(ret.first)), ret.second };
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
    auto& impl = *_data.object;
    auto  ret  = place(impl, impl.lower_bound(key), key, std::move(val));
    return { object_iterator(impl.address_of(ret.first)), ret.second };
}

std::pair<value::object_iterator, bool> value::try_emplace(const std::wstring& key, value val)
{
    check_type(jsonv::kind::object, kind());
    return try_emplace(detail::convert_to_narrow(key), std::move(val));
}

std::pair<value::object_iterator, bool> value::insert_or_assign(const std::string& key, value val)
{
    check_type(jsonv::kind::object, kind());
    auto& impl = *_data.object;
    auto  pos  = impl.lower_bound(key);
    if (pos != impl._members.end() && pos->first == key)
    {
        pos->second = std::move(val);
        return { object_iterator(impl.address_of(pos)), false };
    }
    else
    {
        pos = impl._members.emplace(pos, key, std::move(val));
        return { object_iterator(impl.address_of(pos)), true };
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
    auto& impl = *_data.object;
    auto  pos  = impl.find(key);
    if (pos == impl._members.end())
        return 0U;

    impl._members.erase(pos);
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
    auto& impl = *_data.object;
    return object_iterator(impl.address_of(impl._members.erase(impl.position_of(position._member))));
}

value::object_iterator value::erase(const_object_iterator first, const_object_iterator last)
{
    check_type(jsonv::kind::object, kind());
    auto& impl = *_data.object;
    return object_iterator(impl.address_of(impl._members.erase(impl.position_of(first._member),
                                                               impl.position_of(last._member)
                                                              )
                                          )
                          );
}

object_node_handle value::extract(const_object_iterator position)
{
    check_type(jsonv::kind::object, kind());

    // Being able to rewrite the key of an extracted element is the whole point of a node handle, so the key moves out
    // of the object rather than being copied.
    auto& impl = *_data.object;
    auto  pos  = impl.position_of(position._member);
    object_node_handle out(object_node_handle::purposeful_construction(),
                           std::move(pos->first),
                           std::move(pos->second)
                          );
    impl._members.erase(pos);
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

bool object_impl::empty() const
{
    return _members.empty();
}

value::size_type object_impl::size() const
{
    return _members.size();
}

object_impl::iterator object_impl::position_for(const_iterator hint, std::string_view key)
{
    auto pos = _members.begin() + (hint - _members.cbegin());
    if (  (pos == _members.begin() || std::string_view(std::prev(pos)->first) < key)
       && (pos == _members.end()   || key < std::string_view(pos->first))
       )
        return pos;
    else
        return lower_bound(key);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// object_builder                                                                                                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::pair<object_impl::iterator, bool> object_builder::find_indexed(const index_type& index, std::string_view key)
{
    auto found = index.find(key);
    if (found == index.end())
        return { _members->end(), false };
    else
        return { _members->begin() + static_cast<std::ptrdiff_t>(*found), true };
}

void object_builder::add_indexed(object_impl::iterator where, std::string&& key, value&& val)
{
    // With an index, members are appended; without one, this is the member which goes past `sorted_insert_limit`, and
    // it still goes where it belongs. Either way, `emplace` can not fail once it has moved from its arguments, and if
    // indexing the member then throws, it is taken back out.
    if (_index)
        where = _members->emplace(_members->end(), std::move(key), std::move(val));
    else
        where = _members->emplace(where, std::move(key), std::move(val));

    try
    {
        if (_index)
        {
            _index->insert(std::size_t(where - _members->begin()));
        }
        else
        {
            // The members are in key order so far, so each goes at the end of the index.
            index_type index(key_of_position{ _members });
            for (std::size_t pos = 0U; pos < _members->size(); ++pos)
                index.insert(index.end(), pos);
            _index = std::move(index);
        }
    }
    catch (...)
    {
        key = std::move(where->first);
        val = std::move(where->second);
        _members->erase(where);
        throw;
    }
}

void object_builder::sort_indexed()
{
    // The first `sorted_insert_limit` members were inserted in order and the rest appended. The index refers to them by
    // position, which sorting changes. Every key is distinct, so the order the sort leaves equal keys in does not come
    // up.
    _index.reset();
    std::sort(_members->begin(),
              _members->end(),
              [] (const object_impl::member_type& a, const object_impl::member_type& b)
              {
                  return a.first < b.first;
              }
             );
}

}

}
