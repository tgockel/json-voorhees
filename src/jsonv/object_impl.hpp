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
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "detail/counted_block.hpp"
#include "detail/node_arena.hpp"

namespace jsonv::detail
{

/// The members of an object, kept sorted by key in one contiguous array: what \c value_storage::object points at, or
/// null for an empty object with no room. The members share one allocation with the object's size and capacity, so an
/// object costs a single allocation rather than one per member.
class JSONV_LOCAL object_impl final :
        public counted_block<std::pair<std::string, jsonv::value>, object_impl>
{
public:
    using member_type = std::pair<std::string, jsonv::value>;

    /// Orders a member against a key, for \c std::lower_bound.
    struct key_less
    {
        bool operator()(const member_type& member, std::string_view key) const noexcept
        {
            return std::string_view(member.first) < key;
        }
    };

public:
    static member_type* end(object_impl* self) noexcept
    {
        return data(self) + size(self);
    }

    static const member_type* end(const object_impl* self) noexcept
    {
        return data(self) + size(self);
    }

    /// The first member whose key is not less than \a key.
    static member_type* lower_bound(object_impl* self, std::string_view key)
    {
        return std::lower_bound(data(self), end(self), key, key_less());
    }

    static const member_type* lower_bound(const object_impl* self, std::string_view key)
    {
        return std::lower_bound(data(self), end(self), key, key_less());
    }

    /// The member with \a key, or the end if there is none.
    static member_type* find(object_impl* self, std::string_view key)
    {
        auto pos = lower_bound(self, key);
        return pos != end(self) && pos->first == key ? pos : end(self);
    }

    static const member_type* find(const object_impl* self, std::string_view key)
    {
        auto pos = lower_bound(self, key);
        return pos != end(self) && pos->first == key ? pos : end(self);
    }

    /// Where a member with \a key belongs: \a hint if it is the right place, since then the key needs no search, and
    /// otherwise \ref lower_bound. The member there may already have \a key.
    static member_type* position_for(object_impl* self, const member_type* hint, std::string_view key);

    /// The index of the member at \a pos, which may be the end.
    static size_type index_of(const object_impl* self, const member_type* pos) noexcept
    {
        return static_cast<size_type>(pos - data(self));
    }

private:
    explicit object_impl(size_type capacity) noexcept :
            counted_block(capacity)
    { }

    friend class counted_block<member_type, object_impl>;
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
/// A builder refers to the pointer to its object's members in the \c value holding the object, since adding a member
/// can move the members to a larger block. That \c value must stay where it is until \ref finish. A builder can not be
/// copied: the copy would still fill the original object.
class JSONV_LOCAL object_builder final
{
public:
    using member_type = object_impl::member_type;

public:
    /// Build into \a target, which must have no members, with room for \a expected_size of them.
    explicit object_builder(object_impl*& target, std::size_t expected_size = 0U) :
            _target(&target),
            _expected_size(expected_size)
    {
        object_impl::reserve(target, expected_size);
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
    std::pair<member_type*, bool> find(std::string_view key)
    {
        if (_index)
            return find_indexed(_index->positions, key);

        auto pos = object_impl::lower_bound(*_target, key);
        return { pos, pos != object_impl::end(*_target) && pos->first == key };
    }

    /// Add a member with \a key and \a val at \a where, which \ref find returned for \a key. If this throws, \a key
    /// and \a val are as they were and the object is unchanged.
    void add(member_type* where, std::string&& key, value&& val)
    {
        // `emplace` can not fail once it has moved from its arguments: the only thing which can throw is allocating a
        // larger block, which comes first.
        if (!_index && object_impl::size(*_target) < sorted_insert_limit)
            object_impl::emplace(*_target, object_impl::index_of(*_target, where), std::move(key), std::move(val));
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
        if (object_impl::capacity(*_target) / 2U > object_impl::size(*_target))
            object_impl::shrink_to_fit(*_target);
    }

private:
    /// How many members are inserted in key order before the rest are appended and indexed.
    static constexpr std::size_t sorted_insert_limit = 16U;

    /// Orders positions in the object by the key of the member there, and keys directly, so the index can be searched
    /// with a \c std::string_view. It looks the members up through the \c value's pointer each time, since adding a
    /// member can move them.
    struct key_of_position
    {
        using is_transparent = void;

        object_impl* const* target;

        std::string_view key(std::size_t pos) const noexcept { return object_impl::data(*target)[pos].first; }
        std::string_view key(std::string_view key) const noexcept { return key; }

        template <typename T, typename U>
        bool operator()(const T& a, const U& b) const noexcept
        {
            return key(a) < key(b);
        }
    };

    using index_type = std::set<std::size_t, key_of_position, arena_allocator<std::size_t>>;

    /// The index, and the arena its nodes come from: it only ever gains nodes until \ref finish drops it whole, so they
    /// need not be freed one by one. Held by pointer, so the builder stays movable.
    struct index_storage
    {
        index_storage(key_of_position order, std::size_t expected_size) :
                // A tree node of a std::size_t is the value and three pointers, and a colour the padding absorbs. Two
                // more make room for what a set allocates besides its members: MSVC's sentinel node, and in Debug its
                // iterator proxy.
                arena((expected_size + 2U) * (sizeof(std::size_t) + 4U * sizeof(void*))),
                positions(order, arena_allocator<std::size_t>(arena))
        { }

        node_arena arena;
        index_type positions;
    };

    /// \ref find once there is an \a index.
    std::pair<member_type*, bool> find_indexed(const index_type& index, std::string_view key);

    /// \ref add once there is an index, or when this member is the one which needs it built.
    void add_indexed(member_type* where, std::string&& key, value&& val);

    /// \ref finish once there is an index.
    void sort_indexed();

private:
    object_impl**                  _target;
    std::size_t                    _expected_size;
    std::unique_ptr<index_storage> _index;
};

}
