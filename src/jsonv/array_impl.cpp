/// \file
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "array_impl.hpp"
#include "detail.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>

namespace jsonv
{

value array()
{
    value x;
    x._data.array = nullptr;
    x._kind = jsonv::kind::array;
    return x;
}

value array(std::initializer_list<value> source)
{
    value x = array();
    x.assign(source);
    return x;
}

namespace detail
{

// Moving a `value` can not throw, so once a block is allocated, relocating elements into it can not fail. Every
// function which grows allocates first and then moves, which is what gives them the strong guarantee.

array_impl* array_impl::allocate(size_type capacity)
{
    if (capacity == 0U)
        return nullptr;

    // The same bound `std::vector<value>::max_size` has, less room for the header.
    constexpr size_type max_capacity =
        (static_cast<size_type>(std::numeric_limits<std::ptrdiff_t>::max()) - sizeof(array_impl)) / sizeof(value);
    if (capacity > max_capacity)
        throw std::length_error("no array can hold " + std::to_string(capacity) + " elements");

    void* storage = ::operator new(sizeof(array_impl) + capacity * sizeof(value));
    return ::new (storage) array_impl(capacity);
}

void array_impl::destroy(array_impl* self) noexcept
{
    if (!self)
        return;

    std::destroy_n(self->elements(), self->_size);
    self->~array_impl();
    ::operator delete(static_cast<void*>(self));
}

array_impl* array_impl::copy(const array_impl* source)
{
    if (!source || source->_size == 0U)
        return nullptr;

    array_impl* out = allocate(source->_size);
    try
    {
        const value* from = source->elements();
        for (size_type idx = 0U; idx < source->_size; ++idx)
        {
            ::new (out->elements() + idx) value(from[idx]);
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

array_impl* array_impl::reallocate(array_impl* self, size_type capacity)
{
    array_impl* out = allocate(capacity);
    if (self)
    {
        value* from = self->elements();
        for (size_type idx = 0U; idx < self->_size; ++idx)
            ::new (out->elements() + idx) value(std::move(from[idx]));
        out->_size = self->_size;
        destroy(self);
    }
    return out;
}

array_impl::size_type array_impl::grown_capacity(const array_impl* self, size_type needed) noexcept
{
    // Doubling, as `std::vector` does, so a run of `push_back` moves each element a constant number of times.
    size_type doubled = self ? self->_capacity * 2U : 1U;
    return std::max(doubled, needed);
}

void array_impl::reserve(array_impl*& self, size_type capacity)
{
    if (capacity > (self ? self->_capacity : 0U))
        self = reallocate(self, capacity);
}

void array_impl::push_back_grow(array_impl*& self, value&& item)
{
    // `item` may be one of the elements, which `reallocate` moves from, so take it before that.
    value       taken(std::move(item));
    array_impl* grown;
    try
    {
        grown = reallocate(self, grown_capacity(self, size(self) + 1U));
    }
    catch (...)
    {
        item = std::move(taken);
        throw;
    }

    self = grown;
    ::new (self->elements() + self->_size) value(std::move(taken));
    ++self->_size;
}

void array_impl::insert(array_impl*& self, size_type index, value&& item)
{
    size_type count = size(self);
    if (count == (self ? self->_capacity : 0U))
        self = reallocate(self, grown_capacity(self, count + 1U));

    value* elems = self->elements();
    if (index == count)
    {
        ::new (elems + count) value(std::move(item));
    }
    else
    {
        ::new (elems + count) value(std::move(elems[count - 1U]));
        std::move_backward(elems + index, elems + count - 1U, elems + count);
        elems[index] = std::move(item);
    }
    ++self->_size;
}

void array_impl::assign(array_impl*& self, size_type count, const value& val)
{
    // Built apart and swapped in, so `val` is still there to copy from however it is related to `self`.
    array_impl* out = allocate(count);
    try
    {
        for (size_type idx = 0U; idx < count; ++idx)
        {
            ::new (out->elements() + idx) value(val);
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

void array_impl::assign(array_impl*& self, const value* first, size_type count)
{
    array_impl* out = allocate(count);
    try
    {
        for (size_type idx = 0U; idx < count; ++idx)
        {
            ::new (out->elements() + idx) value(first[idx]);
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

void array_impl::resize(array_impl*& self, size_type count, const value& val)
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

    if (count <= (self ? self->_capacity : 0U))
    {
        // The size stays put until every copy is made, since `val` may be this array or hold it, and each copy has to
        // see the elements it had to begin with.
        value*    elems = self->elements();
        size_type built = old_count;
        try
        {
            for (; built < count; ++built)
                ::new (elems + built) value(val);
        }
        catch (...)
        {
            std::destroy(elems + old_count, elems + built);
            throw;
        }
        self->_size = count;
        return;
    }

    // The new elements are copied into the larger block before the old ones are moved there, since `val` may be one of
    // the old ones. Growing one element at a time has to move each element a constant number of times, as with
    // push_back, so the block grows as push_back's does.
    array_impl* out   = allocate(grown_capacity(self, count));
    value*      elems = out->elements();
    size_type   built = old_count;
    try
    {
        for (; built < count; ++built)
            ::new (elems + built) value(val);
    }
    catch (...)
    {
        std::destroy(elems + old_count, elems + built);
        destroy(out);
        throw;
    }

    value* from = data(self);
    for (size_type idx = 0U; idx < old_count; ++idx)
        ::new (elems + idx) value(std::move(from[idx]));
    out->_size = count;
    destroy(self);
    self = out;
}

void array_impl::erase(array_impl* self, size_type first, size_type last) noexcept
{
    if (first == last)
        return;

    value* elems = self->elements();
    std::move(elems + last, elems + self->_size, elems + first);
    size_type new_size = self->_size - (last - first);
    std::destroy(elems + new_size, elems + self->_size);
    self->_size = new_size;
}

}

value::array_iterator value::begin_array()
{
    check_type(jsonv::kind::array, kind());
    return array_iterator(this, 0);
}

value::const_array_iterator value::begin_array() const
{
    check_type(jsonv::kind::array, kind());
    return const_array_iterator(this, 0);
}

value::array_iterator value::end_array()
{
    check_type(jsonv::kind::array, kind());
    return array_iterator(this, detail::array_impl::size(_data.array));
}

value::const_array_iterator value::end_array() const
{
    check_type(jsonv::kind::array, kind());
    return const_array_iterator(this, detail::array_impl::size(_data.array));
}

value::array_view value::as_array() &
{
    return array_view(begin_array(), end_array());
}

value::const_array_view value::as_array() const &
{
    return const_array_view(begin_array(), end_array());
}

value::owning_array_view value::as_array() &&
{
    check_type(jsonv::kind::array, kind());
    return owning_array_view(std::move(*this),
                             [] (value& x) { return x.begin_array(); },
                             [] (value& x) { return x.end_array(); }
                            );
}

value& value::operator[](size_type idx)
{
    check_type(jsonv::kind::array, kind());
    return detail::array_impl::data(_data.array)[idx];
}

const value& value::operator[](size_type idx) const
{
    check_type(jsonv::kind::array, kind());
    return detail::array_impl::data(_data.array)[idx];
}

namespace
{

void check_index(value::size_type idx, value::size_type size)
{
    if (idx >= size)
        throw std::out_of_range("array index " + std::to_string(idx) + " is out of range for an array of "
                                + std::to_string(size) + " elements");
}

}

value& value::at(size_type idx)
{
    check_type(jsonv::kind::array, kind());
    check_index(idx, detail::array_impl::size(_data.array));
    return detail::array_impl::data(_data.array)[idx];
}

const value& value::at(size_type idx) const
{
    check_type(jsonv::kind::array, kind());
    check_index(idx, detail::array_impl::size(_data.array));
    return detail::array_impl::data(_data.array)[idx];
}

void value::push_back(value&& item)
{
    check_type(jsonv::kind::array, kind());
    detail::array_impl::push_back(_data.array, std::move(item));
}

void value::push_back(const value& item)
{
    check_type(jsonv::kind::array, kind());
    detail::array_impl::push_back(_data.array, value(item));
}

void value::pop_back()
{
    check_type(jsonv::kind::array, kind());
    if (detail::array_impl::size(_data.array) == 0U)
        throw std::logic_error("Cannot pop from empty array");
    detail::array_impl::pop_back(_data.array);
}

value::array_iterator value::insert(const_array_iterator position, value item)
{
    check_type(jsonv::kind::array, kind());
    auto index = static_cast<size_type>(std::distance(const_array_iterator(begin_array()), position));
    detail::array_impl::insert(_data.array, index, std::move(item));
    return begin_array() + static_cast<difference_type>(index);
}

void value::assign(size_type count, const value& val)
{
    check_type(jsonv::kind::array, kind());
    detail::array_impl::assign(_data.array, count, val);
}

void value::assign(std::initializer_list<value> items)
{
    check_type(jsonv::kind::array, kind());
    detail::array_impl::assign(_data.array, items.begin(), items.size());
}

void value::reserve(size_type count)
{
    check_type(jsonv::kind::array, kind());
    detail::array_impl::reserve(_data.array, count);
}

void value::resize(size_type count, const value& val)
{
    check_type(jsonv::kind::array, kind());
    detail::array_impl::resize(_data.array, count, val);
}

value::array_iterator value::erase(const_array_iterator position)
{
    check_type(jsonv::kind::array, kind());
    difference_type dist(position - begin_array());
    auto            index = static_cast<size_type>(dist);
    detail::array_impl::erase(_data.array, index, index + 1U);
    return array_iterator(this, index);
}

value::array_iterator value::erase(const_array_iterator first, const_array_iterator last)
{
    difference_type fdist(first - begin_array());
    difference_type ldist(last  - begin_array());
    detail::array_impl::erase(_data.array, static_cast<size_type>(fdist), static_cast<size_type>(ldist));
    return array_iterator(this, static_cast<size_type>(fdist));
}

}
