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

#include <json-voorhees/config.hpp>

namespace jsonv
{

#define JSONV_VERSION_MAJOR    0
#define JSONV_VERSION_MINOR    1
#define JSONV_VERSION_REVISION 1

/** A basic structure for describing a version of the library. **/
struct library_version
{
    int major;
    int minor;
    int revision;
    
    constexpr library_version(int major, int minor, int revision) :
            major{major},
            minor{minor},
            revision{revision}
    { }
    
    constexpr bool operator==(const library_version& other) const
    {
        return major    == other.major
            && minor    == other.minor
            && revision == other.revision;
    }
    
    constexpr bool operator!=(const library_version& other) const
    {
        return major    != other.major
            || minor    != other.minor
            || revision != other.revision;
    }
};

/** The version of the library you have included from your source. **/
constexpr library_version    included_version = library_version(JSONV_VERSION_MAJOR,
                                                                JSONV_VERSION_MINOR,
                                                                JSONV_VERSION_REVISION
                                                               );

/** The version of the library which you are linking against. Hopefully, this is the same as \c included_version. **/
extern const library_version compiled_version;

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
    return included_version == compiled_version;
}

}

#endif/*__JSON_VOORHEES_STANDARD_HPP_INCLUDED__*/
