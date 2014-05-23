/** \file jsonv/config.hpp
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_CONFIG_HPP_INCLUDED__
#define __JSONV_CONFIG_HPP_INCLUDED__

/** \def JSONV_USER_CONFIG
 *  \brief A user-defined configuration file to be included before all other JsonVoorhees content.
**/
#ifdef JSONV_USER_CONFIG
#   include JSONV_USER_CONFIG
#endif

/** \def JSONV_SO
 *  \brief Are you using shared objects (DLLs in Windows)?
**/
#ifndef JSONV_SO
#   define JSONV_SO 1
#endif

/** \def JSONV_COMPILING
 *  \brief Is JsonVoorhees currently compiling?
 *  You probably do not want to set this by hand. It is set by the Makefile when the library is compiled.
**/
#ifndef JSONV_COMPILING
#   define JSONV_COMPILING 0
#endif

/** \def JSONV_EXPORT
 *  If using shared objects, this class or function should be exported.
 *  
 *  \def JSONV_IMPORT
 *  If using shared objects, this class or function should be imported.
 *  
 *  \def JSONV_HIDDEN
 *  This symbol is only visible within the same shared object in which the translation unit will end up. Symbols which
 *  are "hidden" will \e not be put into the global offset table, which means code can be more optimal when it involves
 *  hidden symbols at the cost that nothing outside of the SO can access it.
**/
#if JSONV_SO
#   if defined(__GNUC__)
#       define JSONV_EXPORT          __attribute__((visibility("default")))
#       define JSONV_IMPORT
#       define JSONV_HIDDEN          __attribute__((visibility("hidden")))
#   elif defined(__MSC_VER)
#       define JSONV_EXPORT          __declspec(dllexport)
#       define JSONV_IMPORT          __declspec(dllimport)
#       define JSONV_HIDDEN
#   else
#       error "Unknown shared object semantics for this compiler."
#   endif
#else
#   define JSONV_EXPORT
#   define JSONV_IMPORT
#   define JSONV_HIDDEN
#endif

/** \def JSONV_PUBLIC
 *  \brief This function or class is part of the public API for JsonVoorhees.
 *  If you are including JsonVoorhees for another library, this will have import semantics (\c JSONV_IMPORT); if you are
 *  building JsonVoorhees, this will have export semantics (\c JSONV_EXPORT).
 *  
 *  \def JSONV_LOCAL
 *  \brief This function or class is internal-use only.
 *  \see JSONV_HIDDEN
**/
#if JSONV_COMPILING
#   define JSONV_PUBLIC JSONV_EXPORT
#   define JSONV_LOCAL  JSONV_HIDDEN
#else
#   define JSONV_PUBLIC JSONV_IMPORT
#   define JSONV_LOCAL  JSONV_HIDDEN
#endif

/** \def JSONV_NO_RETURN
 *  \brief Mark that a given function will never return control to the caller, either by exiting or throwing an
 *  exception.
**/
#ifndef JSONV_NO_RETURN
#   if defined(__GNUC__)
#       define JSONV_NO_RETURN __attribute__((noreturn))
#   else
#       define JSONV_NO_RETURN
#   endif
#endif

/** \def JSONV_ALWAYS_INLINE
 *  \brief Always inline the function this decorates, no matter what the compiler might think is best.
**/
#ifndef JSONV_ALWAYS_INLINE
#   if defined(__GNUC__)
#       define JSONV_ALWAYS_INLINE __attribute__((always_inline))
#   else
#       define JSONV_ALWAYS_INLINE
#   endif
#endif

/** \def JSONV_INTEGER_ALTERNATES_LIST
 *  \brief An item list of types to also consider as an integer.
 *  This mostly exists to help resolve the C-induced type ambiguity for the literal \c 0. It most prefers to be an
 *  \c int, but might also become a \c long or a pointer type.
**/
#ifndef JSONV_INTEGER_ALTERNATES_LIST
#   define JSONV_INTEGER_ALTERNATES_LIST(item) \
        item(int)                              \
        item(unsigned int)
#endif

/** \def JSONV_STRING_REF_TYPE
 *  The type to use for \c jsonv::string_ref. By default, this is \c boost::string_ref (by version 0.3.0, this will be
 *  replaced with a local implementation so there is no dependency on Boost).
 *  
 *  \def JSONV_STRING_REF_INCLUDE
 *  The file to include to get the implementation for \c string_ref. If you define \c JSONV_STRING_REF_TYPE, you should
 *  also define this.
**/
#ifndef JSONV_STRING_REF_TYPE
#   define JSONV_STRING_REF_TYPE    boost::string_ref
#   define JSONV_STRING_REF_INCLUDE <boost/utility/string_ref.hpp>
#endif

#endif/*__JSONV_CONFIG_HPP_INCLUDED__*/
