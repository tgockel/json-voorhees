/** \file jsonv/detail/basic_view.hpp
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_DETAIL_BASIC_VIEW_HPP_INCLUDED__
#define __JSONV_DETAIL_BASIC_VIEW_HPP_INCLUDED__

#include <jsonv/config.hpp>

#include <iterator>

namespace jsonv
{
namespace detail
{

/** A view template used for array and object views of a \c value. This class allows traversing a \c value with a range
 *  based for loop.
 *  
 *  \note
 *  A view does nothing to preserve the lifetime of the underlying container, nor does it remain valid if the container
 *  is modified.
**/
template <typename TIterator,
          typename TConstIterator = TIterator
         >
class basic_view
{
public:
    typedef TIterator                                           iterator;
    typedef TConstIterator                                      const_iterator;
    typedef typename std::iterator_traits<iterator>::value_type value_type;
    typedef typename std::iterator_traits<iterator>::reference  reference;
    typedef typename std::iterator_traits<iterator>::pointer    pointer;
    
public:
    basic_view(iterator begin_, iterator end_) :
            _begin(begin_),
            _end(end_)
    { }
    
    iterator       begin()       { return _begin; }
    const_iterator begin() const { return _begin; }
    iterator       end()         { return _end; }
    const_iterator end() const   { return _end; }
    
    const_iterator cbegin() const { return _begin; }
    const_iterator cend()   const { return _end; }
    
private:
    iterator _begin;
    iterator _end;
};

}
}

#endif/*__JSONV_DETAIL_BASIC_VIEW_HPP_INCLUDED__*/
