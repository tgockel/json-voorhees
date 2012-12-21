/** \file
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <json-voorhees/array.hpp>

#include "detail.hpp"

namespace jsonv
{

using namespace jsonv::detail;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// array                                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define ARR _data.array->_values

array::array()
{
    _data.array = new detail::array_impl;
    _kind = kind::array;
}

array::array(const array& source) :
        value(source)
{ }

array::array(array&& source) :
        value(std::move(source))
{ }

array& array::operator=(const array& source)
{
    value::operator=(source);
    return *this;
}

array& array::operator=(array&& source)
{
    value::operator=(std::move(source));
    return *this;
}

array::size_type array::size() const
{
    return ARR.size();
}

bool array::empty() const
{
    return ARR.empty();
}

array::iterator array::begin()
{
    return iterator(this, 0);
}

array::const_iterator array::begin() const
{
    return const_iterator(this, 0);
}

array::iterator array::end()
{
    return iterator(this, size());
}

array::const_iterator array::end() const
{
    return const_iterator(this, size());
}

value& array::operator[](size_type idx)
{
    return ARR[idx];
}

const value& array::operator[](size_type idx) const
{
    return ARR[idx];
}

void array::push_back(const value& val)
{
   ARR.push_back(val);
}

void array::pop_back()
{
    ARR.pop_back();
}

void array::push_front(const value& val)
{
    ARR.push_front(val);
}

void array::pop_front()
{
    ARR.pop_front();
}

void array::clear()
{
    ARR.clear();
}

void array::resize(size_type count, const value& val)
{
    ARR.resize(count, val);
}

array::iterator array::erase(const_iterator iter)
{
    size_type dist(iter - begin());
    ARR.erase(ARR.begin() + dist);
    return iterator(this, dist);
}

array::iterator array::erase(const_iterator first, const_iterator last)
{
    size_type fdist(first - begin());
    size_type ldist(last  - begin());
    ARR.erase(ARR.begin() + fdist, ARR.begin() + ldist);
    return iterator(this, fdist);
}

bool array::operator ==(const array& other) const
{
    if (this == &other)
        return true;
    if (size() != other.size())
        return false;
    
    typedef array_impl::array_type::const_iterator const_iterator;
    
    const_iterator self_iter  = ARR.begin();
    const_iterator other_iter = other.ARR.begin();
    
    for (const const_iterator self_end = ARR.end();
         self_iter != self_end;
         ++self_iter, ++other_iter
        )
    {
        if (*self_iter != *other_iter)
            return false;
    }
    
    return true;
}

bool array::operator !=(const array& other) const
{
    return !operator==(other);
}

ostream_type& operator <<(ostream_type& stream, const array& arr)
{
    typedef array_impl::array_type::const_iterator const_iterator;
    
    const_iterator iter = arr.ARR.begin();
    const_iterator end  = arr.ARR.end();
    
    stream << "[";
    
    if (iter != end)
    {
        stream << *iter;
        ++iter;
    }
    
    for ( ; iter != end; ++iter)
    {
        stream << ", " << *iter;
    }
    
    stream << "]";
    
    return stream;
}

}
