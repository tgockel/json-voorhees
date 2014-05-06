/** \file jsonv/value.hpp
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_VALUE_HPP_INCLUDED__
#define __JSONV_VALUE_HPP_INCLUDED__

#include "standard.hpp"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace jsonv
{

namespace detail
{

class object_impl;
class array_impl;
class string_impl;

union value_storage
{
    object_impl* object;
    array_impl*  array;
    string_impl* string;
    int64_t      integer;
    double       decimal;
    bool         boolean;
};

}

enum class kind
{
    null,
    object,
    array,
    string,
    integer,
    decimal,
    boolean
};

class JSONV_PUBLIC kind_error :
        public std::logic_error
{
public:
    explicit kind_error(const std::string& description);
    
    virtual ~kind_error() throw();
};

/** Represents a single JSON value, which can be any one of a potential \c kind, each behaving slightly differently.
 *  Instances will vary their behavior based on their kind -- functions will throw a \c kind_error if the operation does
 *  not apply to the value's kind. For example, it does not make sense to call \c find on an \c integer.
 *  
 *   - \c kind::null
 *     You cannot do anything with this...it is just null.
 *   - \c kind::boolean
 *     These values can be \c true or \c false.
 *   - \c kind::integer
 *     A numeric value which can be added, subtracted and all the other things you would expect.
 *   - \c kind::decimal
 *     Floating-point values should be considered "more general" than integers -- you may request an integer value as a
 *     decimal, but you cannot request a decimal as an integer, even when doing so would not require rounding. The
 *     literal \c 20.0 will always have \c kind::decimal.
 *   - \c kind::string
 *     A UTF-8 encoded string which is mostly accessed through the \c std::string class. Some random functions work in
 *     the cases where it makes sense (for example: \c empty and \c size), but in general, string manipulation should be
 *     done after calling \c as_string.
 *   - \c kind::array
 *     An array behaves like a \c std::deque because it is ultimately backed by one. If you feel the documentation is
 *     lacking, read this: http://en.cppreference.com/w/cpp/container/deque.
 *   - \c kind::object
 *     An object behaves lake a \c std::map because it is ultimately backed by one. If you feel the documentation is
 *     lacking, read this: http://en.cppreference.com/w/cpp/container/map. This library follows the recommendation in
 *     RFC 7159 to not allow for duplicate keys because most other libraries can not deal with it. It would also make
 *     the AST significantly more painful.
 *  
 *  \see http://json.org/
 *  \see http://tools.ietf.org/html/rfc7159
**/
class JSONV_PUBLIC value
{
public:
    typedef std::size_t    size_type;
    typedef std::ptrdiff_t difference_type;
    
    /** \addtogroup Array
     *  \{
     *  The base type for iterating over array values.
    **/
    template <typename T, typename TArrayView>
    struct basic_array_iterator :
            public std::iterator<std::random_access_iterator_tag, T>
    {
    public:
        basic_array_iterator() :
                _owner(0),
                _index(0)
        { }
        
        basic_array_iterator(TArrayView* owner, size_type index) :
                _owner(owner),
                _index(index)
        { }
        
        template <typename U, typename UArrayView>
        basic_array_iterator(const basic_array_iterator<U, UArrayView>& source,
                             typename std::enable_if<std::is_convertible<U*, T*>::value>::type* = 0
                            ) :
                _owner(source._owner),
                _index(source._index)
        { }
        
        basic_array_iterator& operator++()
        {
            ++_index;
            return *this;
        }
        
        basic_array_iterator operator++(int) const
        {
            basic_array_iterator clone = *this;
            ++clone;
            return clone;
        }
        
        basic_array_iterator& operator--()
        {
            --_index;
            return *this;
        }
        
        basic_array_iterator operator--(int) const
        {
            basic_array_iterator clone = *this;
            --clone;
            return clone;
        }
        
        template <typename U, typename UArrayView>
        bool operator==(const basic_array_iterator<U, UArrayView>& other) const
        {
            return _owner == other._owner && _index == other._index;
        }
        
        template <typename U, typename UArrayView>
        bool operator!=(const basic_array_iterator<U, UArrayView>& other) const
        {
            return !operator==(other);
        }
        
        T& operator*() const
        {
            return _owner->operator[](_index);
        }
        
        T* operator->() const
        {
            return &_owner->operator[](_index);
        }
        
        basic_array_iterator& operator+=(size_type n)
        {
            _index += n;
            return *this;
        }
        
        basic_array_iterator operator+(size_type n) const
        {
            basic_array_iterator clone = *this;
            clone += n;
            return clone;
        }
        
        basic_array_iterator& operator-=(size_type n)
        {
            _index -= n;
            return *this;
        }
        
        basic_array_iterator operator-(size_type n) const
        {
            basic_array_iterator clone = *this;
            clone -= n;
            return clone;
        }
        
        difference_type operator-(const basic_array_iterator& other) const
        {
            return difference_type(_index) - difference_type(other._index);
        }
        
        bool operator<(const basic_array_iterator& rhs) const
        {
            return _index < rhs._index;
        }
        
        bool operator<=(const basic_array_iterator& rhs) const
        {
            return _index <= rhs._index;
        }
        
        bool operator>(const basic_array_iterator& rhs) const
        {
            return _index > rhs._index;
        }
        
        bool operator>=(const basic_array_iterator& rhs) const
        {
            return _index >= rhs._index;
        }
        
        T& operator[](size_type n) const
        {
            return _owner->operator[](_index + n);
        }
    private:
        template <typename U, typename UArrayView>
        friend struct basic_array_iterator;
        
        friend class value;
        
    private:
        TArrayView* _owner;
        size_type   _index;
    };
    
    typedef basic_array_iterator<value, value>             array_iterator;
    typedef basic_array_iterator<const value, const value> const_array_iterator;
    
    /** \} **/
    
    /** \addtogroup
     *  \{
     *  The base iterator type for iterating over object types. It is a bidirectional iterator similar to a
     *  \c std::map<std::string, jsonv::value>.
    **/
    template <typename T>
    struct basic_object_iterator :
            public std::iterator<std::bidirectional_iterator_tag, T>
    {
    public:
        basic_object_iterator();
        
        basic_object_iterator(const basic_object_iterator& source)
        {
            copy_from(source._storage);
        }
        
        /** This allows assignment from an \c object_iterator to a \c const_object_iterator.
        **/
        template <typename U>
        basic_object_iterator(const basic_object_iterator<U>& source,
                              typename std::enable_if<std::is_convertible<U*, T*>::value>::type* = 0
                             );
        
        basic_object_iterator& operator++()
        {
            increment();
            return *this;
        }
        
        basic_object_iterator operator++(int) const
        {
            basic_object_iterator clone(*this);
            clone.increment();
            return clone;
        }
        
        basic_object_iterator& operator--()
        {
            decrement();
            return *this;
        }
        
        basic_object_iterator operator--(int) const
        {
            basic_object_iterator clone(*this);
            clone.decrement();
            return clone;
        }
        
        template <typename U>
        bool operator ==(const basic_object_iterator<U>& other) const
        {
            return equals(other._storage);
        }
        
        template <typename U>
        bool operator !=(const basic_object_iterator<U>& other) const
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
        friend class value;
        
        template <typename U>
        explicit basic_object_iterator(const U&);
        
        void increment();
        void decrement();
        T&   current() const;
        bool equals(const char* other_storage) const;
        void copy_from(const char* other_storage);
        
    private:
        char _storage[sizeof(void*)];
    };
    
    typedef std::pair<const std::string, value>            object_value_type;
    typedef basic_object_iterator<object_value_type>       object_iterator;
    typedef basic_object_iterator<const object_value_type> const_object_iterator;
    
    /** \} **/
    
public:
    /// Default-construct this to null.
    value();
    value(std::nullptr_t);
    value(const value& source);
    value(const std::string& val);
    value(const char* val);
    value(int64_t val);
    value(double val);
    value(bool val);
    
    #define JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_PROTO_GENERATOR(type_)              \
        value(type_ val);
    JSONV_INTEGER_ALTERNATES_LIST(JSONV_VALUE_INTEGER_ALTERNATIVE_CTOR_PROTO_GENERATOR)
    
    // Don't allow any implicit conversions that we have not specified.
    template <typename T>
    value(T) = delete;
    
    /** Destruction will never throw. **/
    ~value() throw();
    
    /** Copy-assigns \c source to this.
     *  
     *  If an exception is thrown during the copy, it is propagated out. This instance will be set to a null value - any
     *  value will be disposed.
    **/
    value& operator=(const value& source);
    
    /** Move-construct this instance, leaving \a source as a null value. **/
    value(value&& source) throw();
    
    /** Move-assigns \c source to this, leaving \a source as a null value.
     *  
     *  Unlike a copy, this will never throw.
    **/
    value& operator=(value&& source) throw();
    
    /** \addtogroup Conversions
     *  These functions are used for accessing specific kinds of values.
     *  \{
    **/
    
    /** Get this value as a string.
     *  
     *  \throws kind_error if this value does not represent a string.
    **/
    const std::string& as_string() const;
    
    /** Get this value as an integer.
     *  
     *  \throws kind_error if this value does not represent an integer.
    **/
    int64_t as_integer() const;
    
    /** Get this value as a decimal. If the value's underlying kind is actually an integer type, cast the integer to a
     *  double before returning. This ignores the potential loss of precision.
     *  
     *  \throws kind_error if this value does not represent a decimal or integer.
    **/
    double as_decimal() const;
    
    /** Get this value as a boolean.
     *  
     *  \throws kind_error if this value does not represent a boolean.
    **/
    bool as_boolean() const;
    
    /** \} **/
    
    /** \addtogroup Shared
     *  These functions are applicable to all kinds of values and have the same fundemental meaning for all kinds.
     *  \{
    **/
    
    /** Resets this value to null. **/
    void clear();
    
    /** Get this value's kind. **/
    inline kind get_kind() const
    {
        return _kind;
    }
    
    /** Swap the value this instance represents with \a other. **/
    void swap(value& other) throw();
    
    /** Compares two JSON values for equality. Two JSON values are equal if and only if all of the following conditions
     *  apply:
     *  
     *   1. They have the same valid value for \c kind.
     *      - If \c kind is invalid (memory corruption), then two JSON values are \e not equal, even if they have been
     *        corrupted in the same way and even if they share \c this (a corrupt object is not equal to itself).
     *   2. The kind comparison is also equal:
     *      a. Two null values are always equivalent.
     *      b. string, integer, decimal and boolean follow the classic rules for their type.
     *      c. objects are equal if they have the same keys and values corresponding with the same key are also equal.
     *      d. arrays are equal if they have the same length and the values at each index are also equal.
     *  
     *  \note
     *  The rules for equality are based on Python \c dict and \c list.
    **/
    bool operator==(const value& other) const;
    
    /** Compares two JSON values for inequality. The rules for inequality are the exact opposite of equality.
    **/
    bool operator!=(const value& other) const;
    
    /** Used to build a strict-ordering of JSON values. When comparing values of the same kind, the ordering should
     *  align with your intuition. When comparing values of different kinds, some arbitrary rules were created based on
     *  how "complicated" the author thought the type to be.
     *  
     *  null - less than everything but null, which it is equal to.
     *  boolean - false is less than true.
     *  integer, decimal - compared by their numeric value. Comparisons between two integers do not cast, but comparison
     *    between an integer and a decimal will coerce to decimal.
     *  string - compared lexicographically by character code (with basic char strings and non-ASCII encoding, this
     *    might lead to surprising results)
     *  array - compared lexicographically by elements (recursively following this same technique)
     *  object - entries in the object are sorted and compared lexicographically, first by key then by value
     *  
     *  \returns -1 if this is less than other by the rules stated above; 0 if this is equal to other; -1 if otherwise.
    **/
    int compare(const value& other) const;
    
    bool operator< (const value& other) const;
    bool operator> (const value& other) const;
    bool operator<=(const value& other) const;
    bool operator>=(const value& other) const;
    
    friend std::ostream& operator<<(std::ostream& stream, const value& val);
    
    /** \} **/
    
    /** \addtogroup Array
     *  These functions are only applicable if the kind of this value is an array.
     *  \{
    **/
    
    /** Get an iterator to the beginning of this array.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    array_iterator       begin_array();
    const_array_iterator begin_array() const;
    
    /** Get an iterator to the end of this array.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    array_iterator       end_array();
    const_array_iterator end_array() const;
    
    /** Get the value in this array at the given \a idx. The overloads which accept an \c int are required to resolve
     *  the type ambiguity of the literal \c 0 between a size_type and a char*.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    value& operator[](size_type idx);
    const value& operator[](size_type idx) const;
    inline value&       operator[](int idx)       { return operator[](size_type(idx)); }
    inline const value& operator[](int idx) const { return operator[](size_type(idx)); }
    
    /** Get the value in this array at the given \a idx.
     *  
     *  \throws kind_error if the kind is not an array.
     *  \throws std::out_of_range if the provided \a idx is above \c size.
    **/
    value& at(size_type idx);
    const value& at(size_type idx) const;
    
    /** Push \a item to the back of this array.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    void push_back(value item);
    
    /** Pop an item off the back of this array.
     *  
     *  \throws kind_error if the kind is not an array.
     *  \throws std::logic_error if the array is empty.
    **/
    void pop_back();
    
    /** Push \a item to the front of this array.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    void push_front(value item);
    
    /** Pop an item from the front of this array.
     *  
     *  \throws kind_error if the kind is not an array.
     *  \throws std::logic_error if the array is empty.
    **/
    void pop_front();
    
    /** Assign \a count elements to this array with \a val.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    void assign(size_type count, const value& val);
    
    /** Assign the contents of range [\a first, \a last) to this array.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    template <typename TForwardIterator>
    void assign(TForwardIterator first, TForwardIterator last)
    {
        resize(std::distance(first, last), value());
        auto iter = begin_array();
        while (first != last)
        {
            *iter = *first;
            ++iter;
            ++first;
        }
    }
    
    /** Assign the given \a items to this array.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    void assign(std::initializer_list<value> items);
    
    /** Resize the length of this array to \a count items. If the resize creates new elements, fill those newly-created
     *  elements with \a val.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    void resize(size_type count, const value& val = value());
    
    /** Erase the item at this array's \a position.
     * 
     *  \throws kind_error if the kind is not an array.
    **/
    array_iterator erase(const_array_iterator position);
    
    /** Erase the range [\a first, \a last) from this array.
     *  
     *  \throws kind_error if the kind is not an array.
    **/
    array_iterator erase(const_array_iterator first, const_array_iterator last);
    
    /** \}
    **/
    
    /** \addtogroup Object
     *  These functions are only applicable if the kind of this value is an object.
     *  \{
    **/
    
    /** Get an iterator to the first key-value pair in this object.
     *  
     *  \throws kind_error if the kind is not an object.
    **/
    object_iterator       begin_object();
    const_object_iterator begin_object() const;
    
    /** Get an iterator to the one past the end of this object.
     *  
     *  \throws kind_error if the kind is not an object.
    **/
    object_iterator       end_object();
    const_object_iterator end_object() const;
    
    /** Get the value associated with the given \a key of this object. If the \a key does not exist, it will be created.
     *  
     *  \throws kind_error if the kind is not an object.
    **/
    value& operator[](const std::string& key);
    const value& operator[](const std::string& key) const;
    value& operator[](const char* key);
    const value& operator[](const char* key) const;
    
    /** Get the value associated with the given \a key of this object.
     *  
     *  \throws kind_error if the kind is not an object.
     *  \throws std::out_of_range if the \a key is not in this object.
    **/
    value& at(const std::string& key);
    const value& at(const std::string& key) const;
    value& at(const char* key);
    const value& at(const char* key) const;
    
    /** Check if the given \a key exists in this object.
     *  
     *  \throws kind_error if the kind is not an object.
    **/
    size_type count(const std::string& key) const;
    size_type count(const char* key) const;
    
    /** Attempt to locate a key-value pair with the provided \a key in this object.
     *  
     *  \throws kind_error if the kind is not an object.
    **/
    object_iterator       find(const std::string& key);
    const_object_iterator find(const std::string& key) const;
    object_iterator       find(const char* key);
    const_object_iterator find(const char* key) const;
    
    /** Insert \a pair into this object. If \a hint is provided, this insertion could be optimized.
     *  
     *  \returns A pair whose \c first refers to the newly-inserted element (or the element which shares the key).
     *  \throws kind_error if the kind is not an object.
    **/
    std::pair<object_iterator, bool> insert(const std::pair<std::string, value>& pair);
    std::pair<object_iterator, bool> insert(const std::pair<const char*, value>& pair);
    object_iterator insert(const_object_iterator hint, const std::pair<std::string, value>& pair);
    object_iterator insert(const_object_iterator hint, const std::pair<const char*, value>& pair);
    
    /** Insert range defined by [\a first, \a last) into this object.
     *  
     *  \throws kind_error if the kind is not an object.
    **/
    template <typename TForwardIterator>
    void insert(TForwardIterator first, TForwardIterator last)
    {
        for ( ; first != last; ++first)
            insert(first);
    }
    
    /** Insert \a items into this object.
     *  
     *  \throws kind_error if the kind is not an object.
    **/
    void insert(std::initializer_list<object_value_type> items);
    void insert(std::initializer_list<std::pair<const char*, value>> items);
    
    /** Erase the item with the given \a key.
     *  
     *  \returns 1 if \a key was erased; 0 if it did not.
     *  \throws kind_error if the kind is not an object.
    **/
    size_type erase(const std::string& key);
    size_type erase(const char* key);
    
    /** Erase the item at the given \a position.
     *  
     *  \throws kind_error if the kind is not an object.
    **/
    object_iterator erase(const_object_iterator position);
    
    /** Erase the range defined by [\a first, \a last).
     *  
     *  \throws kind_error if the kind is not an object.
    **/
    object_iterator erase(const_object_iterator first, const_object_iterator last);
    
    /** \}
    **/
    
    /** \addtogroup Shared
     *  These functions are only applicable if the kind of this value is an array.
     *  \{
    **/
    
    /** Is the underlying structure empty? This has similar meaning for all types it works on and is always equivalent
     *  to asking if the size is 0.
     *  
     *   - object: Are there no keys?
     *   - array: Are there no values?
     *   - string: Is the string 0 length?
     *  
     *  \throws kind_error if the kind is not an object, array or string.
    **/
    bool empty() const;
    
    /** Get the number of items in this value.
     *  
     *   - object: The number of key/value pairs.
     *   - array: The number of values.
     *   - string: The number of code points in the string (including \c \\0 values and counting multi-byte encodings as
     *             more than one value).
     *  
     *  \throws kind_error if the kind is not an object, array or string.
    **/
    size_type size() const;
    
    /** \}
    **/
    
private:
    friend value array();
    friend value object();
    
private:
    detail::value_storage _data;
    kind                  _kind;
};

/** Swap the values \a a and \a b. **/
JSONV_PUBLIC void swap(value& a, value& b) throw();

/** \addtogroup Creation
 *  Free functions meant for easily creating \c value instances.
 *  \{
**/

/** Create an empty array value. **/
JSONV_PUBLIC value array();

/** Create an array value from the given source. **/
JSONV_PUBLIC value array(std::initializer_list<value> source);

/** Create an array with contents defined by range [\a first, \a last). **/
template <typename TForwardIterator>
value array(TForwardIterator first, TForwardIterator last)
{
    value arr = array();
    arr.assign(first, last);
    return arr;
}

/** Create an empty object. **/
JSONV_PUBLIC value object();

/** Create an object with key-value pairs from the given \a source. **/
JSONV_PUBLIC value object(std::initializer_list<std::pair<const std::string, value>> source);

/** Create an object whose contents are defined by range [\a first, \a last). **/
template <typename TForwardIterator>
value object(TForwardIterator first, TForwardIterator last)
{
    value obj = object();
    obj.insert(first, last);
    return obj;
}

/** \} **/

}

#endif/*__JSONV_VALUE_HPP_INCLUDED__*/
