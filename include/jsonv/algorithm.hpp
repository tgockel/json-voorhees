/** \file jsonv/algorithm.hpp
 *  A collection of algorithms a la \c <algorithm>.
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
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

#include <functional>

namespace jsonv
{

class path;
class value;

/** \addtogroup Algorithm
 *  \{
 *  A collection of useful free functions a la \c <algorithm>.
**/

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

/** \} **/

}

#endif/*__JSONV_ALGORITHM_HPP_INCLUDED__*/
