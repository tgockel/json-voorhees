/// \file
/// Internal-only header for array-related values.
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

#include <new>
#include <utility>

#include "detail/attributes.hpp"

namespace jsonv::detail
{

/// The elements of an array, in one allocation with their count and capacity in front of them. A \c value holding an
/// array points at one of these, so an array costs a single allocation rather than a box and a buffer. An empty array
/// may hold no block at all: \c value_storage::array is then null, and every function here takes null as an array with
/// no elements and no room.
///
/// The functions which can move the elements to a larger block take the pointer by reference and repoint it. Anything
/// holding the address of an element has to look it up again afterwards, as with \c std::vector.
class JSONV_LOCAL array_impl final
{
public:
    using size_type = value::size_type;

public:
    array_impl(const array_impl&)            = delete;
    array_impl& operator=(const array_impl&) = delete;

    static size_type size(const array_impl* self) noexcept
    {
        return self ? self->_size : 0U;
    }

    static value* data(array_impl* self) noexcept
    {
        return self ? self->elements() : nullptr;
    }

    static const value* data(const array_impl* self) noexcept
    {
        return self ? self->elements() : nullptr;
    }

    /// A block with room for \a capacity elements and none in it, or null if \a capacity is 0.
    ///
    /// \throws std::length_error if no block could hold \a capacity elements.
    static array_impl* allocate(size_type capacity);

    /// Destroy the elements of \a self and free it. \a self may be null.
    static void destroy(array_impl* self) noexcept;

    /// A block holding copies of the elements of \a source and no spare room, or null if \a source has no elements.
    static array_impl* copy(const array_impl* source);

    /// Make room for at least \a capacity elements. If this throws, \a self is unchanged.
    static void reserve(array_impl*& self, size_type capacity);

    /// Add \a item to the end, moving the elements to a larger block if there is no room. \a item may be one of the
    /// elements. If this throws, \a self and \a item are unchanged.
    static void push_back(array_impl*& self, value&& item)
    {
        if (!self || self->_size == self->_capacity)
        {
            JSONV_UNLIKELY
            push_back_grow(self, std::move(item));
            return;
        }

        ::new (self->elements() + self->_size) value(std::move(item));
        ++self->_size;
    }

    /// Insert \a item at \a index, which is no more than the size. If this throws, \a self is unchanged.
    static void insert(array_impl*& self, size_type index, value&& item);

    /// Replace the elements with \a count copies of \a val, which may be one of them. If this throws, \a self is
    /// unchanged.
    static void assign(array_impl*& self, size_type count, const value& val);

    /// Replace the elements with copies of the \a count values at \a first, which may be among them. If this throws,
    /// \a self is unchanged.
    static void assign(array_impl*& self, const value* first, size_type count);

    /// Make the size \a count, removing elements from the end or adding copies of \a val there, which may be one of
    /// the elements. If this throws, \a self is unchanged.
    static void resize(array_impl*& self, size_type count, const value& val);

    /// Remove the elements in [\a first, \a last), moving those after them down.
    static void erase(array_impl* self, size_type first, size_type last) noexcept;

    /// Remove the last element, which must exist.
    static void pop_back(array_impl* self) noexcept
    {
        --self->_size;
        self->elements()[self->_size].~value();
    }

private:
    array_impl(size_type capacity) noexcept :
            _size(0U),
            _capacity(capacity)
    { }

    ~array_impl() noexcept = default;

    value* elements() noexcept
    {
        return std::launder(reinterpret_cast<value*>(this + 1));
    }

    const value* elements() const noexcept
    {
        return std::launder(reinterpret_cast<const value*>(this + 1));
    }

    /// Move the elements of \a self to a new block with room for \a capacity, at least the size, and free \a self.
    static array_impl* reallocate(array_impl* self, size_type capacity);

    /// The capacity to grow to when \a self is full and needs room for \a needed elements.
    static size_type grown_capacity(const array_impl* self, size_type needed) noexcept;

    /// \ref push_back when there is no room.
    static void push_back_grow(array_impl*& self, value&& item);

private:
    size_type _size;
    size_type _capacity;
};

static_assert(sizeof(array_impl) % alignof(value) == 0U, "elements must be aligned right after the header");
static_assert(alignof(array_impl) >= alignof(value), "the header's alignment must suit the elements");

}
