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
    detail::array_impl::emplace_back(_data.array, std::move(item));
}

void value::push_back(const value& item)
{
    check_type(jsonv::kind::array, kind());
    detail::array_impl::emplace_back(_data.array, item);
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
    detail::array_impl::emplace(_data.array, index, std::move(item));
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
    detail::array_impl::assign_range(_data.array, items.begin(), items.size());
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
