/** \file jsonv/array.hpp
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_ARRAY_HPP_INCLUDED__
#define __JSONV_ARRAY_HPP_INCLUDED__

#include "value.hpp"

namespace jsonv
{

class JSONV_PUBLIC array :
        public value
{
public:
    typedef std::size_t    size_type;
    typedef std::ptrdiff_t difference_type;
    typedef value          value_type;
    typedef value&         reference;
    typedef const value&   const_reference;
    typedef value*         pointer;
    typedef const value*   const_pointer;
    
    template <typename T, typename TArrayView>
    struct basic_iterator :
            public std::iterator<std::random_access_iterator_tag, T>
    {
    public:
        basic_iterator() :
                _owner(0),
                _index(0)
        { }
        
        basic_iterator(TArrayView* owner, size_type index) :
                _owner(owner),
                _index(index)
        { }
        
        template <typename U, typename UArrayView>
        basic_iterator(const basic_iterator<U, UArrayView>& source,
                       typename std::enable_if<std::is_convertible<U*, T*>::value>::type* = 0
                      ) :
                _owner(source._owner),
                _index(source._index)
        { }
        
        basic_iterator& operator ++()
        {
            ++_index;
            return *this;
        }
        
        basic_iterator operator ++(int) const
        {
            basic_iterator clone = *this;
            ++clone;
            return clone;
        }
        
        basic_iterator& operator --()
        {
            --_index;
            return *this;
        }
        
        basic_iterator operator --(int) const
        {
            basic_iterator clone = *this;
            --clone;
            return clone;
        }
        
        template <typename U, typename UArrayView>
        bool operator ==(const basic_iterator<U, UArrayView>& other) const
        {
            return _owner == other._owner && _index == other._index;
        }
        
        template <typename U, typename UArrayView>
        bool operator !=(const basic_iterator<U, UArrayView>& other) const
        {
            return !operator==(other);
        }
        
        T& operator *() const
        {
            return _owner->operator[](_index);
        }
        
        T* operator ->() const
        {
            return &_owner->operator[](_index);
        }
        
        basic_iterator& operator +=(size_type n)
        {
            _index += n;
            return *this;
        }
        
        basic_iterator operator +(size_type n) const
        {
            basic_iterator clone = *this;
            clone += n;
            return clone;
        }
        
        basic_iterator& operator -=(size_type n)
        {
            _index -= n;
            return *this;
        }
        
        basic_iterator operator -(size_type n) const
        {
            basic_iterator clone = *this;
            clone -= n;
            return clone;
        }
        
        difference_type operator -(const basic_iterator& other) const
        {
            return difference_type(_index) - difference_type(other._index);
        }
        
        bool operator <(const basic_iterator& rhs) const
        {
            return _index < rhs._index;
        }
        
        bool operator <=(const basic_iterator& rhs) const
        {
            return _index <= rhs._index;
        }
        
        bool operator >(const basic_iterator& rhs) const
        {
            return _index > rhs._index;
        }
        
        bool operator >=(const basic_iterator& rhs) const
        {
            return _index >= rhs._index;
        }
        
        T& operator[](size_type n) const
        {
            return _owner->operator[](_index + n);
        }
    private:
        template <typename U, typename UArrayView>
        friend struct basic_iterator;
        
    private:
        TArrayView* _owner;
        size_type   _index;
    };
    
    typedef basic_iterator<value, array>             iterator;
    typedef basic_iterator<const value, const array> const_iterator;
    
public:
    array();
    array(const array& source);
    array(array&& source);
    
    array& operator=(const array& source);
    array& operator=(array&& source);
    
public:
    size_type size() const;
    bool empty() const;
    
    iterator begin();
    const_iterator begin() const;
    
    iterator end();
    const_iterator end() const;
    
    value& operator[](size_type idx);
    const value& operator[](size_type idx) const;
    
    void push_back(const value& value);
    void pop_back();
    
    void push_front(const value& value);
    void pop_front();
    
    void clear();
    void resize(size_type count, const value& val = value());
    
    iterator erase(const_iterator position);
    iterator erase(const_iterator first, const_iterator last);
    
    void push_back_many()
    { }
    
    template <typename T, typename... TRest>
    void push_back_many(T&& val, TRest&&... rest)
    {
        push_back(std::forward<T>(val));
        push_back_many(std::forward<TRest>(rest)...);
    }
    
    friend std::ostream& operator <<(std::ostream& stream, const array& arr);
    
    /// \see value::operator==
    bool operator==(const array& other) const;
    using value::operator==;
    
    /// \see value::operator!=
    bool operator!=(const array& other) const;
    using value::operator!=;
    
    // delete all the non-array casts -- they're not valid
    object& as_object() = delete;
    const object& as_object() const = delete;
    std::string& as_string() = delete;
    const std::string& as_string() const = delete;
    int64_t& as_integer() = delete;
    int64_t  as_integer() const = delete;
    double& as_decimal() = delete;
    double  as_decimal() const = delete;
    bool& as_boolean() = delete;
    bool  as_boolean() const = delete;
    
private:
    // NO ADDED MEMBERS
};

/** Creates an array with the given \a values.
 *  
 *  \example
 *  \code
 *  jsonv::value val = jsonv::make_array(1, 2, 3, "foo");
 *  // Creates: [1, 2, 3, "foo"]
 *  \endcode
**/
template <typename... T>
array make_array(T&&... values)
{
    array arr;
    arr.push_back_many(std::forward<T>(values)...);
    return arr;
}

}

#endif/*__JSONV_ARRAY_HPP_INCLUDED__*/
