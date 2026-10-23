/// \file
/// Internal-only header for object-related values.
///
/// Copyright (c) 2014-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/value.hpp>
#include <jsonv/detail.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "detail/cloneable.hpp"

namespace jsonv::detail
{

/// The members of an object, kept sorted by key in one contiguous array. Every member is one element of \c _members, so
/// an object costs a single allocation for all of them, rather than one per member.
class JSONV_LOCAL object_impl :
        public cloneable<object_impl>
{
public:
    using member_type    = std::pair<std::string, jsonv::value>;
    using members_type   = std::vector<member_type>;
    using iterator       = members_type::iterator;
    using const_iterator = members_type::const_iterator;

    /// Orders a member against a key, for \c std::lower_bound.
    struct key_less
    {
        bool operator()(const member_type& member, std::string_view key) const noexcept
        {
            return std::string_view(member.first) < key;
        }
    };

public:
    bool empty() const;

    value::size_type size() const;

    /// The first member whose key is not less than \a key.
    iterator lower_bound(std::string_view key)
    {
        return std::lower_bound(_members.begin(), _members.end(), key, key_less());
    }

    const_iterator lower_bound(std::string_view key) const
    {
        return std::lower_bound(_members.begin(), _members.end(), key, key_less());
    }

    /// The member with \a key, or the end if there is none.
    iterator find(std::string_view key)
    {
        auto pos = lower_bound(key);
        return pos != _members.end() && pos->first == key ? pos : _members.end();
    }

    const_iterator find(std::string_view key) const
    {
        auto pos = lower_bound(key);
        return pos != _members.end() && pos->first == key ? pos : _members.end();
    }

    /// Where a member with \a key belongs: \a hint if it is the right place, since then the key needs no search, and
    /// otherwise \ref lower_bound. The member there may already have \a key.
    iterator position_for(const_iterator hint, std::string_view key);

    /// The address of the member at \a pos, which is what a \c value::object_iterator holds. \a pos may be the end.
    member_type* address_of(const_iterator pos) noexcept
    {
        return _members.data() + (pos - _members.cbegin());
    }

    /// The position of the member at \a address, which came from \ref address_of.
    iterator position_of(const member_type* address) noexcept
    {
        return _members.begin() + (address - _members.data());
    }

public:
    members_type _members;
};

/// Fills an empty object with members which arrive in no particular order. Inserting into a sorted array one member at
/// a time shifts the members after each, which on a large object whose keys arrive in descending order is quadratic.
///
/// While there are few members, each is inserted where it belongs, which for a small object is cheaper than anything
/// else. Past \c sorted_insert_limit of them, the rest are appended and found through an index ordered by key, and
/// \ref finish sorts them once at the end.
///
/// The index is a tree rather than a hash table because the keys come from the document. \c std::hash is not keyed,
/// so a document can be made of keys which all hash alike, and every lookup in a hash index would then walk all of
/// them. A tree bounds each lookup at \c O(log n) whatever the keys, as the \c std::map objects were once stored in
/// did.
///
/// A builder refers to its object's storage, not to the \c value holding it, so moving that \c value leaves the
/// builder valid. It can not be copied: the copy would still fill the original object.
class JSONV_LOCAL object_builder final
{
public:
    /// Build into \a target, which must have no members, with room for \a expected_size of them.
    explicit object_builder(object_impl& target, std::size_t expected_size = 0U) :
            _members(&target._members)
    {
        _members->reserve(expected_size);
    }

    object_builder(const object_builder&)            = delete;
    object_builder& operator=(const object_builder&) = delete;
    object_builder(object_builder&&)                 = default;
    object_builder& operator=(object_builder&&)      = default;

    // The members of most objects are inserted in order, so that is done here, where the parser can inline it. Only
    // indexing is done out of line.

    /// Look \a key up among the members added so far.
    ///
    /// \returns The member with \a key and \c true, or where \ref add puts it and \c false. The position is good until
    ///  the next call to \ref add.
    std::pair<object_impl::iterator, bool> find(std::string_view key)
    {
        if (_index)
            return find_indexed(*_index, key);

        auto pos = std::lower_bound(_members->begin(), _members->end(), key, object_impl::key_less());
        return { pos, pos != _members->end() && pos->first == key };
    }

    /// Add a member with \a key and \a val at \a where, which \ref find returned for \a key. If this throws, \a key
    /// and \a val are as they were and the object is unchanged.
    void add(object_impl::iterator where, std::string&& key, value&& val)
    {
        // `emplace` can not fail once it has moved from its arguments: the only thing which can throw is allocating a
        // larger array, which comes first.
        if (!_index && _members->size() < sorted_insert_limit)
            _members->emplace(where, std::move(key), std::move(val));
        else
            add_indexed(where, std::move(key), std::move(val));
    }

    /// Put the members in key order. Nothing can be added after this.
    ///
    /// If more than half the room reserved for members went unused, which only repeated keys can cause when the
    /// builder was told how many members to expect, the room is given back: the object may live long after the text
    /// it came from, and `{"a":1,"a":1,...}` is one member however long it is.
    void finish()
    {
        if (_index)
            sort_indexed();
        if (_members->capacity() / 2U > _members->size())
            _members->shrink_to_fit();
    }

private:
    /// How many members are inserted in key order before the rest are appended and indexed.
    static constexpr std::size_t sorted_insert_limit = 16U;

    /// Orders positions in \c _members by the key of the member there, and keys directly, so the index can be
    /// searched with a \c std::string_view.
    struct key_of_position
    {
        using is_transparent = void;

        const object_impl::members_type* members;

        std::string_view key(std::size_t pos) const noexcept { return (*members)[pos].first; }
        std::string_view key(std::string_view key) const noexcept { return key; }

        template <typename T, typename U>
        bool operator()(const T& a, const U& b) const noexcept
        {
            return key(a) < key(b);
        }
    };

    using index_type = std::set<std::size_t, key_of_position>;

    /// \ref find once there is an \a index.
    std::pair<object_impl::iterator, bool> find_indexed(const index_type& index, std::string_view key);

    /// \ref add once there is an index, or when this member is the one which needs it built.
    void add_indexed(object_impl::iterator where, std::string&& key, value&& val);

    /// \ref finish once there is an index.
    void sort_indexed();

private:
    object_impl::members_type* _members;
    std::optional<index_type>  _index;
};

}
