/** \file jsonv/algorithm.hpp
 *  A collection of algorithms a la `&lt;algorithm&gt;`.
 *  
 *  Copyright (c) 2014-2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_ALGORITHM_HPP_INCLUDED__
#define __JSONV_ALGORITHM_HPP_INCLUDED__

#include <jsonv/config.hpp>
#include <jsonv/value.hpp>
#include <jsonv/path.hpp>

#include <cmath>
#include <functional>
#include <limits>

namespace jsonv
{

class path;

/** \addtogroup Algorithm
 *  \{
 *  A collection of useful free functions a la \c <algorithm>.
**/

/** Traits describing how to perform various aspects of comparison. This implementation for comparison is strict and is
 *  ultimately the one used by \c value::compare.
 *  
 *  \see compare
**/
struct JSONV_PUBLIC compare_traits
{
    /** Compare two kinds \a a and \a b. This should return 0 if the types are the same or if they are directly
     *  comparable (such as \c kind::integer and \c kind::decimal) -- if you return 0 for non-comparable types, you risk
     *  getting a \c kind_error thrown.
    **/
    static int compare_kinds(kind a, kind b)
    {
        int va = kindval(a);
        int vb = kindval(b);
        return va == vb ? 0 : va < vb ? -1 : 1;
    }
    
    /** Compare two boolean values. **/
    static int compare_booleans(bool a, bool b)
    {
        return a == b ?  0
             : a      ?  1
             :          -1;
    }
    
    /** Compare two integer values. **/
    static int compare_integers(std::int64_t a, std::int64_t b)
    {
        return a == b ?  0
             : a <  b ? -1
             :           1;
    }

    /** Compare two decimal values. **/
    static int compare_decimals(double a, double b)
    {
        return (std::abs(a - b) < (std::numeric_limits<double>::denorm_min() * 10.0)) ?  0
             : (a < b)                                                                ? -1
             :                                                                           1;
    }
    
    /** Compare two string values. **/
    static int compare_strings(const std::string& a, const std::string& b)
    {
        return a.compare(b);
    }
    
    /** Compare two strings used for the keys of objects. **/
    static int compare_object_keys(const std::string& a, const std::string& b)
    {
        return a.compare(b);
    }
    
    /** Compare two objects \e before comparing the values. The \c compare function will only check the contents of an
     *  object if this function returns 0.
    **/
    static int compare_objects_meta(const value&, const value&)
    {
        return 0;
    }
    
private:
    static int kindval(kind k)
    {
        switch (k)
        {
        case jsonv::kind::null:
            return 0;
        case jsonv::kind::boolean:
            return 1;
        case jsonv::kind::integer:
        case jsonv::kind::decimal:
            return 2;
        case jsonv::kind::string:
            return 3;
        case jsonv::kind::array:
            return 4;
        case jsonv::kind::object:
            return 5;
        default:
            return -1;
        }
    }
};

/** Compare the values \a a and \a b using the comparison \a traits.
 *  
 *  \tparam TCompareTraits A type which should be compatible with the public signatures on the \c compare_traits class.
**/
template <typename TCompareTraits>
int compare(const value& a, const value& b, const TCompareTraits& traits)
{
    if (&a == &b)
        return 0;
    
    if (int kindcmp = traits.compare_kinds(a.kind(), b.kind()))
        return kindcmp;
    
    switch (a.kind())
    {
    case jsonv::kind::null:
        return 0;
    case jsonv::kind::boolean:
        return traits.compare_booleans(a.as_boolean(), b.as_boolean());
    case jsonv::kind::integer:
        // b might be a decimal type, but if they are both integers, compare directly
        if (b.kind() == jsonv::kind::integer)
            return traits.compare_integers(a.as_integer(), b.as_integer());
        // fall through
    case jsonv::kind::decimal:
        return traits.compare_decimals(a.as_decimal(), b.as_decimal());
    case jsonv::kind::string:
        return traits.compare_strings(a.as_string(), b.as_string());
    case jsonv::kind::array:
    {
        auto aiter = a.begin_array();
        auto biter = b.begin_array();
        for ( ; aiter != a.end_array() && biter != b.end_array(); ++aiter, ++biter)
            if (int cmp = compare(*aiter, *biter, traits))
                return cmp;
        return aiter == a.end_array() ? biter == b.end_array() ? 0 : -1
                                      : 1;
    }
    case jsonv::kind::object:
    {
        if (int objmetacmp = traits.compare_objects_meta(a, b))
            return objmetacmp;
        
        auto aiter = a.begin_object();
        auto biter = b.begin_object();
        for ( ; aiter != a.end_object() && biter != b.end_object(); ++aiter, ++biter)
        {
            if (int cmp = traits.compare_object_keys(aiter->first, biter->first))
                return cmp;
            if (int cmp = compare(aiter->second, biter->second, traits))
                return cmp;
        }
        return aiter == a.end_object() ? biter == b.end_object() ? 0 : -1
                                       : 1;
    }
    default:
        return -1;
    }
}

/** Compare the values \a a and \a b with strict comparison traits.
 *  
 *  \see value::compare
 *  \see compare_icase
**/
JSONV_PUBLIC int compare(const value& a, const value& b);

/** Compare the values \a a and \a b, but use case-insensitive matching on \c kind::string values. This does \e not use
 *  case-insensitive matching on the keys of objects!
 *  
 *  \see compare
**/
JSONV_PUBLIC int compare_icase(const value& a, const value& b);

/** The results of the \c diff operation. **/
struct JSONV_PUBLIC diff_result
{
    /** Elements that were the same between the two halves of the diff. **/
    value same;

    /** Elements that were unique to the left hand side of the diff. **/
    value left;

    /** Elements that were unique to the right hand side of the diff. **/
    value right;
};

/** Find the differences and similarities between the structures of \a left and \a right. If \a left and \a right have
 *  a different \c kind (and the kind difference is not \c kind::integer and \c kind::decimal), \a left and \a right
 *  will be placed directly in the result. If they have the same \c kind and it is scalar, the values get a direct
 *  comparison. If they are the same, the result is moved to \c diff_result::same. If they are different, \a left and
 *  \a right are moved to \c diff_result::left and \c diff_result::right, respectively. For \c kind::array and
 *  \c kind::object, the \c value elements are compared recursively.
**/
JSONV_PUBLIC diff_result diff(value left, value right);

/** Run a function over the values in the \a input. The behavior of this function is different, depending on the \c kind
 *  of \a input. For scalar kinds (\c kind::integer, \c kind::null, etc), \a func is called once with the value. If
 *  \a input is \c kind::array, \c func is called for every value in the array and the output will be an array with each
 *  element transformed by \a func. If \a input is \c kind::object, the result will be an object with each key
 *  transformed by \a func.
 *  
 *  \param func The function to apply to the element or elements of \a input.
 *  \param input The value to transform.
**/
JSONV_PUBLIC value map(const std::function<value (const value&)>& func,
                       const value&                               input
                      );

/** Run a function over the values in the \a input. The behavior of this function is different, depending on the \c kind
 *  of \a input. For scalar kinds (\c kind::integer, \c kind::null, etc), \a func is called once with the value. If
 *  \a input is \c kind::array, \c func is called for every value in the array and the output will be an array with each
 *  element transformed by \a func. If \a input is \c kind::object, the result will be an object with each key
 *  transformed by \a func.
 *  
 *  \param func The function to apply to the element or elements of \a input.
 *  \param input The value to transform.
 *  
 *  \note
 *  This version of \c map provides only a basic exception-safety guarantee. If an exception is thrown while
 *  transforming a non-scalar \c kind, there is no rollback action, so \a input is left in a usable, but
 *  \e unpredictable state. If you need a strong exception guarantee, use the version of \c map that takes a constant
 *  reference to a \c value.
**/
JSONV_PUBLIC value map(const std::function<value (value)>& func,
                       value&&                             input
                      );

/** Recursively walk the provided \a tree and call \a func for each item in the tree.
 *  
 *  \param tree The JSON value to traverse.
 *  \param func The function to call for each element in the tree.
 *  \param base_path The path to prepend to each output path to \a func. This can be useful if beginning traversal from
 *                   inside of some JSON structure.
 *  \param leafs_only If true, call \a func only when the current path is a "leaf" value (\c string, \c integer,
 *                    \c decimal, \c boolean, or \c null \e or an empty \c array or \c object); if false, call \a func
 *                    for all entries in the tree.
**/
JSONV_PUBLIC void traverse(const value&                                           tree,
                           const std::function<void (const path&, const value&)>& func,
                           const path&                                            base_path,
                           bool                                                   leafs_only = false
                          );

/** Recursively walk the provided \a tree and call \a func for each item in the tree.
 *  
 *  \param tree The JSON value to traverse.
 *  \param func The function to call for each element in the tree.
 *  \param leafs_only If true, call \a func only when the current path is a "leaf" value (\c string, \c integer,
 *                    \c decimal, \c boolean, or \c null \e or an empty \c array or \c object); if false, call \a func
 *                    for all entries in the tree.
**/
JSONV_PUBLIC void traverse(const value&                                           tree,
                           const std::function<void (const path&, const value&)>& func,
                           bool                                                   leafs_only = false
                          );

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

/** Error thrown when an unrepresentable value is encountered in a JSON AST.
 *
 *  \see validate
**/
class JSONV_PUBLIC validation_error :
        public std::runtime_error
{
public:
    /** Special code for describing the error encountered. **/
    enum class code
    {
        /** Encountered a number which is NaN or Infinity. **/
        non_finite_number
    };

public:
    explicit validation_error(code code_, jsonv::path path_, jsonv::value value_);

    virtual ~validation_error() noexcept;

    /** Get the error code. **/
    code error_code() const;

    /** Get the path in the AST the error was found. **/
    const jsonv::path& path() const;

    /** Get the value that caused the error. **/
    const jsonv::value& value() const;

private:
    code         _code;
    jsonv::path  _path;
    jsonv::value _value;
};

JSONV_PUBLIC std::ostream& operator<<(std::ostream& os, const validation_error::code& code);

/** Check that the provided \a val is perfectly representable as a JSON string. The JSON specification does not have
 *  support for things like non-finite floating-point numbers (\c NaN and \c infinity). This means \c value defined with
 *  these values will get serialized as \c null. This constitutes a loss of information, but not acting this way would
 *  lead to the encoder outputting invalid JSON text, which is completely unacceptable. Use this funciton to check that
 *  there will be no information loss when encoding.
 *
 *  \throws validation_error if \a val contains an unrepresentable value.
**/
JSONV_PUBLIC void validate(const value& val);

/** \} **/

}

#endif/*__JSONV_ALGORITHM_HPP_INCLUDED__*/
