/** \file jsonv/util.hpp
 *  A collection of utility functions for manipulating one or more instances of \c jsonv::value.
 *  
 *  Copyright (c) 2015 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_UTIL_HPP_INCLUDED__
#define __JSONV_UTIL_HPP_INCLUDED__

#include <jsonv/config.hpp>
#include <jsonv/path.hpp>
#include <jsonv/value.hpp>

#include <functional>
#include <string>
#include <utility>

namespace jsonv
{

/** \addtogroup Utility
 *  \{
 *  Utility functions useful for manipulating instances of \c jsonv::value.
**/

/** This class is used in \c merge_explicit for defining what the function should do in the cases of conflicts. **/
class JSONV_PUBLIC merge_rules
{
public:
    virtual ~merge_rules() noexcept;
    
    /** Called when merging a \c kind::object and the two objects share a key. The implementation can either throw or
     *  merge the keys.
     *  
     *  \param current_path is the merge \c path with the key that conflicted appended.
     *  \param a is the left-hand \c value to merge.
     *  \param b is the right-hand \c value to merge.
    **/
    virtual value resolve_same_key(path&& current_path, value&& a, value&& b) const = 0;
    
    /** Called when \a a and \a b have \c kind values which are incompatible for merging. The implementation can either
     *  throw or coerce a merge.
     * 
     *  \param current_path \c path with the conflicting \c kind values.
     *  \param a is the left-hand \c value to merge.
     *  \param b is the right-hand \c value to merge.
    **/
    virtual value resolve_type_conflict(path&& current_path, value&& a, value&& b) const = 0;
};

/** An implementation of \c merge_rules that allows you to bind whatever functions you want to resolve conflicts. **/
class JSONV_PUBLIC dynamic_merge_rules :
        public merge_rules
{
public:
    using same_key_function =      std::function<value (path&&, value&&, value&&)>;
    
    using type_conflict_function = std::function<value (path&&, value&&, value&&)>;
    
public:
    dynamic_merge_rules(same_key_function      same_key,
                        type_conflict_function type_conflict
                       );
    
    virtual ~dynamic_merge_rules() noexcept;
    
    same_key_function same_key;
    
    type_conflict_function type_conflict;
    
    /// \see merge_rules::resolve_same_key
    virtual value resolve_same_key(path&& current_path, value&& a, value&& b) const override;
    
    /// \see merge_rules::resolve_type_conflict
    virtual value resolve_type_conflict(path&& current_path, value&& a, value&& b) const override;
};

/** These rules throw an exception on all conflicts. **/
class JSONV_PUBLIC throwing_merge_rules :
        public merge_rules
{
public:
    /** \throws std::logic_error **/
    virtual value resolve_same_key(path&& current_path, value&& a, value&& b) const override;
    
    /** \throws kind_error **/
    virtual value resolve_type_conflict(path&& current_path, value&& a, value&& b) const override;
};

/** These rules will recursively merge everything they can and coerce all values. **/
class JSONV_PUBLIC recursive_merge_rules :
        public merge_rules
{
public:
    /** Recursively calls \c merge_explicit with the two values. **/
    virtual value resolve_same_key(path&& current_path, value&& a, value&& b) const override;
    
    /** Calls \c coerce_merge to combine the values. **/
    virtual value resolve_type_conflict(path&& current_path, value&& a, value&& b) const override;
};

/** Merges two \c values, \a a and \a b into a single \c value.
 *  
 *  The merging follows a few simple rules:
 *  
 *   - If \a a.kind() != \a b.kind() and they are not \c kind::integer and \c kind::decimal, call \a on_type_conflict
 *     and return the result.
 *   - Otherwise, branch based on the (shared) type:
 *     - \c kind::object - Return a new object with all the values from \a a and \a b for the keys which are unique per
 *       object. For the keys which are shared, the value is the result of \a on_same_key.
 *     - \c kind::array - Return a new array with the values of \a b appended to \a a.
 *     - \c kind::string - Return a new string with \a b appended to \a a.
 *     - \c kind::boolean - Return `a.as_boolean() || b.as_boolean()`
 *     - \c kind::integer - If \b is \c kind::integer, return `a + b` as an integer; otherwise, return it as a decimal.
 *     - \c kind::decimal - Return `a + b` as a decimal.
 *  
 *  \param rules are the rules to merge with (see \c merge_rules).
 *  \param current_path The current \c path into the \c value that we are merging. This can be used to give more useful
 *                      error information if we are merging recursively.
 *  \param a is a \c value to merge.
 *  \param b is a \c value to merge.
**/
JSONV_PUBLIC value merge_explicit(const merge_rules& rules,
                                  path               current_path,
                                  value              a,
                                  value              b
                                 );

JSONV_PUBLIC value merge_explicit(const merge_rules&, const path&, value a);

JSONV_PUBLIC value merge_explicit(const merge_rules&, const path&);

template <typename... TValue>
value merge_explicit(const merge_rules& rules, path current_path, value a, value b, value c, TValue&&... rest)
{
    value ab = merge_explicit(rules, current_path, std::move(a), std::move(b));
    return merge_explicit(rules,
                          std::move(current_path),
                          std::move(ab),
                          std::move(c),
                          std::forward<TValue>(rest)...
                         );
}

/** Merges all the provided \a values into a single \c value. If there are any key or type conflicts, an exception will
 *  be thrown.
**/
template <typename... TValue>
value merge(TValue&&... values)
{
    return merge_explicit(throwing_merge_rules(),
                          path(),
                          std::forward<TValue>(values)...
                         );
}

/** Merges all the provided \a values into a single \c value. If there are any keys which are shared, their values are
 *  also merged.
**/
template <typename... TValue>
value merge_recursive(TValue&&... values)
{
    return merge_explicit(recursive_merge_rules(),
                          path(),
                          std::forward<TValue>(values)...
                         );
}

/** \} **/

}

#endif/*__JSONV_UTIL_HPP_INCLUDED__*/
