/** \file
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_CHAR_CONVERT_HPP_INCLUDED__
#define __JSONV_CHAR_CONVERT_HPP_INCLUDED__

#include <jsonv/standard.hpp>

#include <stdexcept>

namespace jsonv
{
namespace detail
{

class decode_error :
        public std::runtime_error
{
public:
    typedef std::string::size_type size_type;
public:
    decode_error(size_type offset, const std::string& message);
    
    virtual ~decode_error() throw();
    
    inline size_type offset() const
    {
        return _offset;
    }
    
private:
    size_type _offset;
};

/** Encodes C++ string \a source into a fully-escaped JSON string into \a stream ready for sending over the wire.
**/
std::ostream& string_encode(std::ostream& stream, const std::string& source);

/** Decodes over the wire character sequence \c source into a C++ string. **/
std::string string_decode(const char* source, std::string::size_type source_size);

}
}

#endif/*__JSONV_CHAR_CONVERT_HPP_INCLUDED__*/
