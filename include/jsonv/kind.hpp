/** \file jsonv/kind.hpp
 *
 *  Copyright (c) 2019-2020 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_KIND_HPP_INCLUDED__
#define __JSONV_KIND_HPP_INCLUDED__

#include <jsonv/config.hpp>

#include <iosfwd>
#include <string>

namespace jsonv
{

/** \ingroup Value
 *  \{
**/

/** Describes the \e kind of data a \c value holds. See \c value for more information.
 *
 *  \see http://json.org/
**/
enum class kind : unsigned char
{
    null,
    object,
    array,
    string,
    integer,
    decimal,
    boolean,
};

/** Print out the name of the \c kind. **/
JSONV_PUBLIC std::ostream& operator<<(std::ostream&, const kind&);

/** Get the name of the \c kind. **/
JSONV_PUBLIC std::string to_string(const kind&);

/** \} **/

}

#endif/*__JSONV_KIND_HPP_INCLUDED__*/
