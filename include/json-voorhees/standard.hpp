/** \file json-voorhees/standard.hpp
 *  
 *  Copyright (c) 2012-2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSON_VOORHEES_STANDARD_HPP_INCLUDED__
#define __JSON_VOORHEES_STANDARD_HPP_INCLUDED__

#include <istream>
#include <string>
#include <ostream>

namespace jsonv
{

#define JSON_VOORHEES_VERSION 20140423

extern const unsigned long version_number;

/** \def JSON_VOORHEES_CHAR_TYPE
 *  Sets the type of char used for keys in JSON objects and in JSON string values. By default, this will be \c char.
 *  If you would like to use a different type of character, define this to be whatever you want, but make sure the type
 *  is the same when you compile the library and when you consume the library.
 *  
 *  \warning
 *  I have only tested this library with \c char and will probably fall flat on its face if you try to change to anyhing
 *  else. This option is only present to give you a false sense of the ability to change to some other storage. You also
 *  probably want to keep this as \c char so that you can use \c std::string and all the other things you are used to.
 *  Frankly, any good operating system should support printing multi-byte characters, so the use of changing away from
 *  \c char is even more debatable (if not downright stupid). Furthermore, this library makes over nine thousand
 *  assumptions that you are passing it UTF8-encoded strings from C++land, so if you change that, you will have to fix
 *  any place that assumption is used for profit (namely \c detail::string_decode and \c detail::string_encode). Lastly,
 *  the JSON specification provides a way to transmit Unicode as a sequence of ASCII characters, so there is little gain
 *  from doing Unicode support at all. Even more lastly, while the JSON specification says that a string is "a sequence
 *  of zero or more Unicode characters," many clients will not be happy when they receive Unicode in all its native
 *  glory and bombard your output with question marks of doom or explode (and I figured seeing "ol\uc3a9" in an
 *  application is better than seeing "ol??"...the former is fixable).
**/
#ifndef JSON_VOORHEES_CHAR_TYPE
#   define JSON_VOORHEES_CHAR_TYPE char
#endif

typedef JSON_VOORHEES_CHAR_TYPE char_type;

/** This is the size of \c char_type when the library was built and should be the same as the \c char_type of the
 *  system consuming this.
**/
extern const unsigned long char_type_size;

#ifndef JSON_VOORHEES_INTEGER_ALTERNATES_LIST
#   define JSON_VOORHEES_INTEGER_ALTERNATES_LIST(item) \
        item(int)          \
        item(unsigned int)
#endif

/** Does a non-robust form of checking that the right version and configuration of the library was linked and loaded.
 *  
 *  \returns \c true if we think we are consistent; \c false if otherwise.
**/
inline bool is_consistent()
{
    return version_number == JSON_VOORHEES_VERSION
        && char_type_size == sizeof(char_type);
}

/** The type of string to use in C++. This is the type of keys in JSON objects and the representation of string values.
**/
typedef std::basic_string<char_type>  string_type;

typedef std::basic_ostream<char_type> ostream_type;
typedef std::basic_istream<char_type> istream_type;

}

#endif/*__JSON_VOORHEES_STANDARD_HPP_INCLUDED__*/
