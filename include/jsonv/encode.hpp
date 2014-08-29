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
    virtual void write_null() = 0;
    
    virtual void write_object_begin() = 0;
    
    virtual void write_object_end() = 0;
    
    virtual void write_object_key(string_ref key) = 0;
    
    virtual void write_object_delimiter() = 0;
    
    virtual void write_array_begin() = 0;
    
    virtual void write_array_end() = 0;
    
    virtual void write_array_delimiter() = 0;
    
    virtual void write_string(string_ref value) = 0;
    
    virtual void write_integer(std::int64_t value) = 0;
    
    virtual void write_decimal(double value) = 0;
    
    virtual void write_boolean(bool value) = 0;
};

class JSONV_PUBLIC ostream_encoder :
        public encoder
{
public:
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
    
    virtual void write_decimal(double value) override;
    
    virtual void write_boolean(bool value) override;
    
private:
    std::ostream& _output;
};

}

#endif/*__JSONV_ENCODE_HPP_INCLUDED__*/
