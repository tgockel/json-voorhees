/** \file jsonv/detail/shared_buffer.cpp
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/detail/shared_buffer.hpp>

#include <cstdlib>
#include <cstring>

namespace jsonv
{
namespace detail
{

shared_buffer::shared_buffer() :
        _data(),
        _length(0)
{ }

static std::shared_ptr<char> allocate_block(shared_buffer::size_type size)
{
    char* ptr = static_cast<char*>(std::malloc(size));
    if (!ptr)
        throw std::bad_alloc();
    return std::shared_ptr<char>(ptr, std::free);
}

shared_buffer::shared_buffer(size_type size) :
        _data(allocate_block(size)),
        _length(size)
{ }

shared_buffer::shared_buffer(const_pointer src, size_type size) :
        _data(allocate_block(size)),
        _length(size)
{
    std::memcpy(_data.get(), src, size);
}

shared_buffer::shared_buffer(const shared_buffer& other, bool copy_now) :
        _data(other._data),
        _length(other._length)
{
    if (copy_now)
        make_unique();
}

shared_buffer& shared_buffer::operator=(const shared_buffer&) = default;

shared_buffer::shared_buffer(shared_buffer&& other) throw():
        _data(std::move(other._data)),
        _length(other._length)
{
    other._length = 0UL;
}

shared_buffer& shared_buffer::operator=(shared_buffer&& other) throw()
{
    _data = std::move(other._data);
    _length = other._length;
    other._length = 0UL;
    
    return *this;
}

shared_buffer::~shared_buffer() throw() = default;

shared_buffer shared_buffer::create_zero_filled(size_type size)
{
    shared_buffer buffer(size);
    std::memset(buffer._data.get(), 0, size);
    return buffer;
}

bool shared_buffer::make_unique()
{
    if (is_unique())
    {
        return false;
    }
    else
    {
        shared_buffer newval(_length);
        std::memcpy(newval._data.get(), _data.get(), _length);
        swap(newval);
        return true;
    }
}

void shared_buffer::swap(shared_buffer& other) throw()
{
    using std::swap;
    swap(_data,   other._data);
    swap(_length, other._length);
}

shared_buffer shared_buffer::slice(size_type start_idx, size_type end_idx) const
{
    if (start_idx == npos)
        start_idx = 0;
    if (end_idx == npos)
        end_idx = _length;
    
    if (start_idx > end_idx)
        throw std::invalid_argument("start_idx must be less than or equal to end_idx");
    
    size_type newlen = end_idx - start_idx;
    ensure_index(start_idx, newlen);
    
    // if the result of the slice is 0, just return an empty
    if (start_idx == end_idx)
        return shared_buffer();
    
    shared_buffer newbuff;
    newbuff._data = std::shared_ptr<char>(_data, _data.get() + start_idx);
    newbuff._length = newlen;
    return newbuff;
}

bool shared_buffer::contents_equal(const shared_buffer& other) const
{
    if (this == &other || *this == other)
        return true;
    else if (size() != other.size())
        return false;
    else
        return std::memcmp(get(), other.get(), size()) == 0;
}

}
}
