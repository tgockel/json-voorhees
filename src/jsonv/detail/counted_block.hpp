/// \file
/// Internal-only header for the storage arrays and objects share.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "attributes.hpp"

namespace jsonv::detail
{

/// Elements in one allocation with their count and capacity in front of them, so a container costs a single
/// allocation rather than a box and a buffer. \c TSelf derives from this, adds no data members, and is what a \c value
/// points at: \c array_impl and \c object_impl.
///
/// A container may hold no block at all. Every function here takes a null \c TSelf* as one with no elements and no
/// room, and the functions which can move the elements to a larger block take the pointer by reference and repoint it.
/// Anything holding the address of an element has to look it up again afterwards, as with \c std::vector.
///
/// Moving a \c TElement can not throw, so once a block is allocated, relocating elements into it can not fail. Every
/// function which grows allocates first and then moves, which is what gives them the strong guarantee.
template <typename TElement, typename TSelf>
class counted_block
{
public:
    using size_type = std::size_t;

public:
    counted_block(const counted_block&)            = delete;
    counted_block& operator=(const counted_block&) = delete;

    static size_type size(const TSelf* self) noexcept
    {
        return self ? self->_size : 0U;
    }

    static size_type capacity(const TSelf* self) noexcept
    {
        return self ? self->_capacity : 0U;
    }

    static TElement* data(TSelf* self) noexcept
    {
        return self ? self->elements() : nullptr;
    }

    static const TElement* data(const TSelf* self) noexcept
    {
        return self ? self->elements() : nullptr;
    }

    /// A block with room for \a capacity elements and none in it, or null if \a capacity is 0.
    ///
    /// \throws std::length_error if no block could hold \a capacity elements.
    JSONV_NOINLINE
    static TSelf* allocate(size_type capacity)
    {
        static_assert(sizeof(TSelf) == sizeof(counted_block), "the elements follow the header directly");
        static_assert(sizeof(TSelf) % alignof(TElement) == 0U, "elements must be aligned right after the header");
        static_assert(alignof(TSelf) >= alignof(TElement), "the header's alignment must suit the elements");
        static_assert(std::is_nothrow_move_constructible_v<TElement> && std::is_nothrow_move_assignable_v<TElement>,
                      "relocating the elements must not fail");

        if (capacity == 0U)
            return nullptr;

        // The same bound `std::vector<TElement>::max_size` has, less room for the header.
        constexpr size_type max_capacity =
            (static_cast<size_type>(std::numeric_limits<std::ptrdiff_t>::max()) - sizeof(TSelf)) / sizeof(TElement);
        if (capacity > max_capacity)
            throw std::length_error("no container can hold " + std::to_string(capacity) + " elements");

        void* storage = ::operator new(sizeof(TSelf) + capacity * sizeof(TElement));
        return ::new (storage) TSelf(capacity);
    }

    /// Destroy the elements of \a self and free it. \a self may be null.
    static void destroy(TSelf* self) noexcept
    {
        if (!self)
            return;

        std::destroy_n(self->elements(), self->_size);
        deallocate(self);
    }

    /// Free \a self, which may be null, leaving the elements in it alone: for a caller which has already destroyed
    /// them itself. Destroying them some other way is the only reason to call this rather than \ref destroy.
    static void deallocate(TSelf* self) noexcept
    {
        if (!self)
            return;

        self->~TSelf();
        ::operator delete(static_cast<void*>(self));
    }

    /// A block holding copies of the elements of \a source and no spare room, or null if \a source has no elements.
    static TSelf* copy(const TSelf* source)
    {
        return copy_of(data(source), size(source));
    }

    /// Make room for at least \a capacity elements. If this throws, \a self is unchanged.
    static void reserve(TSelf*& self, size_type capacity)
    {
        if (capacity > counted_block::capacity(self))
            self = reallocate(self, capacity);
    }

    /// Give back the room past the last element, if there is any to give back and a smaller block can be had.
    static void shrink_to_fit(TSelf*& self) noexcept
    {
        if (size(self) == capacity(self))
            return;

        try
        {
            TSelf* smaller = reallocate(self, self->_size);
            self = smaller;
        }
        catch (...) // NOLINT(bugprone-empty-catch): the larger block is kept, which is what shrink_to_fit may do
        { }
    }

    /// Construct an element from \a args at \a index, which is no more than the size, moving the elements from there on
    /// up by one. \a args may refer to elements. If this throws, \a self is unchanged.
    ///
    /// \returns The new element.
    template <typename... TArgs>
    static TElement* emplace(TSelf*& self, size_type index, TArgs&&... args)
    {
        size_type count = size(self);
        if (count == capacity(self))
        {
            JSONV_UNLIKELY
            return emplace_grow(self, index, std::forward<TArgs>(args)...);
        }

        TElement* elems = self->elements();
        if (index == count)
        {
            ::new (elems + count) TElement(std::forward<TArgs>(args)...);
        }
        else
        {
            // Built before anything shifts, since the shift moves what `args` may refer to.
            TElement item(std::forward<TArgs>(args)...);
            ::new (elems + count) TElement(std::move(elems[count - 1U]));
            std::move_backward(elems + index, elems + count - 1U, elems + count);
            elems[index] = std::move(item);
        }
        ++self->_size;
        return elems + index;
    }

    /// \ref emplace at the end. This is how the parser fills every array, so it is kept apart from \ref emplace, whose
    /// shifting would otherwise keep it from being inlined.
    template <typename... TArgs>
    static TElement* emplace_back(TSelf*& self, TArgs&&... args)
    {
        size_type count = size(self);
        if (count == capacity(self))
        {
            JSONV_UNLIKELY
            return emplace_grow(self, count, std::forward<TArgs>(args)...);
        }

        TElement* slot = self->elements() + count;
        ::new (slot) TElement(std::forward<TArgs>(args)...);
        ++self->_size;
        return slot;
    }

    /// Replace the elements with \a count copies of \a val, which may be one of them. If this throws, \a self is
    /// unchanged.
    static void assign(TSelf*& self, size_type count, const TElement& val)
    {
        // Built apart and swapped in, so `val` is still there to copy from however it is related to `self`.
        TSelf* out = allocate(count);
        try
        {
            for (size_type idx = 0U; idx < count; ++idx)
            {
                ::new (out->elements() + idx) TElement(val);
                ++out->_size;
            }
        }
        catch (...)
        {
            destroy(out);
            throw;
        }
        destroy(self);
        self = out;
    }

    /// Replace the elements with copies of the \a count elements at \a first, which may be among them. If this throws,
    /// \a self is unchanged.
    static void assign_range(TSelf*& self, const TElement* first, size_type count)
    {
        TSelf* out = copy_of(first, count);
        destroy(self);
        self = out;
    }

    /// Make the size \a count, removing elements from the end or adding copies of \a val there, which may be one of
    /// the elements. If this throws, \a self is unchanged.
    static void resize(TSelf*& self, size_type count, const TElement& val)
    {
        size_type old_count = size(self);
        if (count <= old_count)
        {
            if (count < old_count)
            {
                std::destroy(self->elements() + count, self->elements() + old_count);
                self->_size = count;
            }
            return;
        }

        if (count <= capacity(self))
        {
            // The size stays put until every copy is made, since `val` may be the container or hold it, and each copy
            // has to see the elements it had to begin with.
            TElement* elems = self->elements();
            size_type built = old_count;
            try
            {
                for (; built < count; ++built)
                    ::new (elems + built) TElement(val);
            }
            catch (...)
            {
                std::destroy(elems + old_count, elems + built);
                throw;
            }
            self->_size = count;
            return;
        }

        // The new elements are copied into the larger block before the old ones are moved there, since `val` may be one
        // of the old ones. Growing one element at a time has to move each element a constant number of times, as with
        // `emplace_back`, so the block grows as it does.
        TSelf*    out   = allocate(grown_capacity(self, count));
        TElement* elems = out->elements();
        size_type built = old_count;
        try
        {
            for (; built < count; ++built)
                ::new (elems + built) TElement(val);
        }
        catch (...)
        {
            std::destroy(elems + old_count, elems + built);
            destroy(out);
            throw;
        }

        relocate(data(self), old_count, elems);
        out->_size = count;
        destroy(self);
        self = out;
    }

    /// Remove the elements in [\a first, \a last), moving those after them down.
    static void erase(TSelf* self, size_type first, size_type last) noexcept
    {
        if (first == last)
            return;

        TElement* elems = self->elements();
        std::move(elems + last, elems + self->_size, elems + first);
        size_type new_size = self->_size - (last - first);
        std::destroy(elems + new_size, elems + self->_size);
        self->_size = new_size;
    }

    /// Remove the last element, which must exist.
    static void pop_back(TSelf* self) noexcept
    {
        --self->_size;
        std::destroy_at(self->elements() + self->_size);
    }

protected:
    explicit counted_block(size_type capacity) noexcept :
            _size(0U),
            _capacity(capacity)
    { }

    ~counted_block() noexcept = default;

private:
    TElement* elements() noexcept
    {
        return std::launder(reinterpret_cast<TElement*>(static_cast<TSelf*>(this) + 1));
    }

    const TElement* elements() const noexcept
    {
        return std::launder(reinterpret_cast<const TElement*>(static_cast<const TSelf*>(this) + 1));
    }

    /// Move-construct the \a count elements at \a from into the uninitialized \a to. The elements at \a from are left
    /// moved-from, for their block's \ref destroy.
    static void relocate(TElement* from, size_type count, TElement* to) noexcept
    {
        for (size_type idx = 0U; idx < count; ++idx)
            ::new (to + idx) TElement(std::move(from[idx]));
    }

    static TSelf* copy_of(const TElement* first, size_type count)
    {
        TSelf* out = allocate(count);
        try
        {
            for (size_type idx = 0U; idx < count; ++idx)
            {
                ::new (out->elements() + idx) TElement(first[idx]);
                ++out->_size;
            }
        }
        catch (...)
        {
            destroy(out);
            throw;
        }
        return out;
    }

    /// Move the elements of \a self to a new block with room for \a capacity, at least the size, and free \a self.
    JSONV_NOINLINE
    static TSelf* reallocate(TSelf* self, size_type capacity)
    {
        TSelf* out = allocate(capacity);
        if (out)
        {
            relocate(data(self), size(self), out->elements());
            out->_size = size(self);
        }
        destroy(self);
        return out;
    }

    /// The capacity to grow to when \a self is full and needs room for \a needed elements: doubling, as
    /// \c std::vector does, so a run of \ref emplace_back moves each element a constant number of times.
    static size_type grown_capacity(const TSelf* self, size_type needed) noexcept
    {
        return std::max(self ? self->_capacity * 2U : 1U, needed);
    }

    /// \ref emplace when there is no room. The larger block is allocated, and the new element built in it, before
    /// anything is moved, so \a args can refer to elements, and if either throws nothing has changed.
    template <typename... TArgs>
    JSONV_NOINLINE
    static TElement* emplace_grow(TSelf*& self, size_type index, TArgs&&... args)
    {
        size_type count = size(self);
        TSelf*    out   = allocate(grown_capacity(self, count + 1U));
        TElement* elems = out->elements();
        try
        {
            ::new (elems + index) TElement(std::forward<TArgs>(args)...);
        }
        catch (...)
        {
            destroy(out);
            throw;
        }

        TElement* from = data(self);
        relocate(from, index, elems);
        relocate(from + index, count - index, elems + index + 1U);
        out->_size = count + 1U;
        destroy(self);
        self = out;
        return elems + index;
    }

private:
    size_type _size;
    size_type _capacity;
};

}
