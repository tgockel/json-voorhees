/** \file json-voorhees/object.hpp
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSON_VOORHEES_OBJECT_HPP_INCLUDED__
#define __JSON_VOORHEES_OBJECT_HPP_INCLUDED__

#include "standard.hpp"
#include "value.hpp"

namespace jsonv
{

class object :
        public value
{
public:
    typedef size_t                              size_type;
    typedef string_type                         key_type;
    typedef value                               mapped_type;
    typedef std::pair<const string_type, value> value_type;
    typedef std::equal_to<string_type>          key_compare;
    typedef value_type&                         reference;
    typedef const value_type&                   const_reference;
    typedef value_type*                         pointer;
    typedef const value_type*                   const_pointer;
    
    template <typename T>
    struct basic_iterator :
            public std::iterator<std::bidirectional_iterator_tag, T>
    {
    public:
        basic_iterator();
        
        basic_iterator(const basic_iterator& source)
        {
            copy_from(source._storage);
        }
        
        /** This allows assignment from an \c iterator to a \c const_iterator.
        **/
        template <typename U>
        basic_iterator(const basic_iterator<U>& source,
                       typename std::enable_if<std::is_convertible<U*, T*>::value>::type* = 0
                      );
        
        basic_iterator& operator++()
        {
            increment();
            return *this;
        }
        
        basic_iterator operator++(int) const
        {
            basic_iterator clone(*this);
            clone.increment();
            return clone;
        }
        
        basic_iterator& operator--()
        {
            decrement();
            return *this;
        }
        
        basic_iterator operator--(int) const
        {
            basic_iterator clone(*this);
            clone.decrement();
            return clone;
        }
        
        template <typename U>
        bool operator ==(const basic_iterator<U>& other) const
        {
            return equals(other._storage);
        }
        
        template <typename U>
        bool operator !=(const basic_iterator<U>& other) const
        {
            return !equals(other._storage);
        }
        
        T& operator *() const
        {
            return current();
        }
        
        T* operator ->() const
        {
            return &current();
        }
        
    private:
        friend class object;
        
        template <typename U>
        explicit basic_iterator(const U&);
        
        void increment();
        void decrement();
        T&   current() const;
        bool equals(const char* other_storage) const;
        void copy_from(const char* other_storage);
        
    private:
        char _storage[sizeof(void*)];
    };
    
    typedef basic_iterator<value_type>       iterator;
    typedef basic_iterator<const value_type> const_iterator;
    
public:
    object();
    object(const object& source);
    object(object&& source);
    
    object& operator =(const object& source);
    object& operator =(object&& source);
    
public:
    size_type size() const;
    
    iterator begin();
    const_iterator begin() const;
    
    iterator end();
    const_iterator end() const;
    
    value& operator[](const string_type& key);
    const value& operator[](const string_type& key) const;
    
    iterator find(const string_type& key);
    const_iterator find(const string_type& key) const;
    
    void insert(const value_type& pair);
    
    /// \see value::operator==
    bool operator==(const object& other) const;
    using value::operator ==;
    
    /// \see value::operator!=
    bool operator!=(const object& other) const;
    using value::operator !=;
    
    friend ostream_type& operator <<(ostream_type& stream, const object& obj);
    
    // delete all the non-object casts -- they're not valid
    array& as_array() = delete;
    const array& as_array() const = delete;
    string_type& as_string() = delete;
    const string_type& as_string() const = delete;
    int64_t& as_integer() = delete;
    int64_t  as_integer() const = delete;
    double& as_decimal() = delete;
    double  as_decimal() const = delete;
    bool& as_boolean() = delete;
    bool  as_boolean() const = delete;
    
private:
    void ensure_object() const;
    
private:
    // NO ADDED MEMBERS
};


namespace detail
{

inline void make_object_impl(jsonv::object&)
{ }

template <typename TKey, typename TValue, typename... TRest>
void make_object_impl(jsonv::object& obj, TKey&& key, TValue&& value, TRest&&... rest)
{
    obj.insert(object::value_type(std::forward<TKey>(key), std::forward<TValue>(value)));
    make_object_impl(obj, std::forward<TRest>(rest)...);
}

}

/** Creates an object with the given \a entries.
 *  
 *  \example
 *  \code
 *  jsonv::value val = jsonv::make_object("foo", 8, "bar", "wat");
 *  // Creates: { "bar": "wat", "foo": 8 }
 *  \endcode
**/
template <typename... T>
object make_object(T&&... entries)
{
    static_assert(sizeof...(T) % 2 == 0, "Must have even number of entries: (key0, value0, key1, value1, ...)");
    
    object obj;
    detail::make_object_impl(obj, std::forward<T>(entries)...);
    return obj;
}

}

#endif/*__JSON_VOORHEES_OBJECT_HPP_INCLUDED__*/
