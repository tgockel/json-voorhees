/** \file jsonv/encode.hpp
 *  Classes and functions for encoding JSON values to various representations.
 *  
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_ENCODE_HPP_INCLUDED__
#define __JSONV_ENCODE_HPP_INCLUDED__

#include <jsonv/config.hpp>
#include <jsonv/forward.hpp>
#include <jsonv/string_ref.hpp>

#include <iosfwd>

namespace jsonv
{

/** An encoder is responsible for writing values to some form of output.
**/
class JSONV_PUBLIC encoder
{
public:
    virtual ~encoder() noexcept;
    
    /** Encode some source value into this encoder. This is the only useful entry point to this class. **/
    void encode(const jsonv::value& source);
    
protected:
    /** Write the null value.
     *  
     *  \code
     *  null
     *  \endcode
    **/
    virtual void write_null() = 0;
    
    /** Write the opening of an object value.
     *  
     *  \code
     *  {
     *  \endcode
    **/
    virtual void write_object_begin() = 0;
    
    /** Write the closing of an object value.
     *  
     *  \code
     *  }
     *  \endcode
    **/
    virtual void write_object_end() = 0;
    
    /** Write the key for an object, including the separator.
     *  
     *  \code
     *  "key":
     *  \endcode
    **/
    virtual void write_object_key(string_ref key) = 0;
    
    /** Write the delimiter between two entries in an object.
     *  
     *  \code
     *  ,
     *  \endcode
    **/
    virtual void write_object_delimiter() = 0;
    
    /** Write the opening of an array value.
     *  
     *  \code
     *  [
     *  \endcode
    **/
    virtual void write_array_begin() = 0;
    
    /** Write the closing of an array value.
     *  
     *  \code
     *  ]
     *  \endcode
    **/
    virtual void write_array_end() = 0;
    
    /** Write the delimiter between two entries in an array.
     *  
     *  \code
     *  ,
     *  \endcode
    **/
    virtual void write_array_delimiter() = 0;
    
    /** Write a string value.
     *  
     *  \param value is the string to write. It will \e hopefully be encoded as valid UTF-8. It is the implementation's
     *               choice of how to deal with malformed string values. Two common options are to replace malformed
     *               sequences with ?s or to simply output these encodings and let the receiver deal with them.
     *  
     *  \code
     *  "value"
     *  \endcode
    **/
    virtual void write_string(string_ref value) = 0;
    
    /** Write an integer value.
     *  
     *  \code
     *  902
     *  \endcode
    **/
    virtual void write_integer(std::int64_t value) = 0;
    
    /** Write a decimal value.
     *  
     *  \param value is the decimal to write. Keep in mind that standard JSON does not support special IEEE 754 values
     *               such as NaN and infinity. It is the implementation's choice of how to deal with such values. Two
     *               common options are to output \c null or to encode a string description of the special value.
     *  
     *  \code
     *  4.9
     *  \endcode
    **/
    virtual void write_decimal(double value) = 0;
    
    /** Write a boolean value.
     *  
     *  \code
     *  true
     *  \endcode
    **/
    virtual void write_boolean(bool value) = 0;
};

/** An encoder that outputs to an \c std::ostream. This implementation is used for \c operator<< on a \c value.
**/
class JSONV_PUBLIC ostream_encoder :
        public encoder
{
public:
    /** Create an instance which places text into \a output. **/
    explicit ostream_encoder(std::ostream& output);
    
    virtual ~ostream_encoder() noexcept;
    
protected:
    virtual void write_null() override;
    
    virtual void write_object_begin() override;
    
    virtual void write_object_end() override;
    
    virtual void write_object_key(string_ref key) override;
    
    virtual void write_object_delimiter() override;
    
    virtual void write_array_begin() override;
    
    virtual void write_array_end() override;
    
    virtual void write_array_delimiter() override;
    
    virtual void write_string(string_ref value) override;
    
    virtual void write_integer(std::int64_t value) override;
    
    /** When a special value is given, this will output \c null. **/
    virtual void write_decimal(double value) override;
    
    virtual void write_boolean(bool value) override;
    
private:
    std::ostream& _output;
};

}

#endif/*__JSONV_ENCODE_HPP_INCLUDED__*/
