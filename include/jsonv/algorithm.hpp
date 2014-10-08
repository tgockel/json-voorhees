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

/** Recursively walk the provided \a tree and call \a func for each item in the tree.
 *  
 *  \param tree The JSON value to traverse.
 *  \param func The function to call for each element in the tree.
 *  \param base_path The path to prepend to each output path to \a func. This can be useful if beginning traversal from
 *                   inside of some JSON structure.
 *  \param leafs_only If true, call \a func only when the current path is a "leaf" value (\c string, \c integer,
 *                    \c decimal, \c boolean or \c null); if false, call \a func for all entries in the tree.
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
 *                    \c decimal, \c boolean or \c null); if false, call \a func for all entries in the tree.
**/
JSONV_PUBLIC void traverse(const value&                                           tree,
                           const std::function<void (const path&, const value&)>& func,
                           bool                                                   leafs_only = false
                          );

/** \} **/

}

#endif/*__JSONV_ALGORITHM_HPP_INCLUDED__*/
