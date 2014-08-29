/** \file
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
#include <jsonv/encode.hpp>
#include <jsonv/value.hpp>

#include "detail.hpp"

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// encoder                                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

encoder::~encoder() noexcept = default;

void encoder::encode(const value& source)
{
    switch (source.get_kind())
    {
    case kind::array:
        write_array_begin();
        {
            bool first = true;
            for (const value& sub : source.as_array())
            {
                if (first)
                    first = false;
                else
                    write_array_delimiter();
                encode(sub);
            }
        }
        write_array_end();
        break;
    case kind::boolean:
        write_boolean(source.as_boolean());
        break;
    case kind::decimal:
        write_decimal(source.as_decimal());
        break;
    case kind::integer:
        write_integer(source.as_integer());
        break;
    case kind::null:
        write_null();
        break;
    case kind::object:
        write_object_begin();
        {
            bool first = true;
            for (const value::object_value_type& entry : source.as_object())
            {
                if (first)
                    first = false;
                else
                    write_object_delimiter();
                
                write_object_key(entry.first);
                encode(entry.second);
            }
        }
        write_object_end();
        break;
    case kind::string:
        write_string(source.as_string());
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ostream_encoder                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ostream_encoder::ostream_encoder(std::ostream& output) :
        _output(output)
{ }

ostream_encoder::~ostream_encoder() noexcept = default;

void ostream_encoder::write_array_begin()
{
    _output << '[';
}

void ostream_encoder::write_array_end()
{
    _output << ']';
}

void ostream_encoder::write_array_delimiter()
{
    _output << ',';
}

void ostream_encoder::write_boolean(bool value)
{
    _output << (value ? "true" : "false");
}

void ostream_encoder::write_decimal(double value)
{
    if (std::isnormal(value))
        _output << value;
    else
        // not a number isn't a valid JSON value, so put it as null
        write_null();
}

void ostream_encoder::write_integer(std::int64_t value)
{
    _output << value;
}

void ostream_encoder::write_null()
{
    _output << "null";
}

void ostream_encoder::write_object_begin()
{
    _output << '{';
}

void ostream_encoder::write_object_end()
{
    _output << '}';
}

void ostream_encoder::write_object_delimiter()
{
    _output << ',';
}

void ostream_encoder::write_object_key(string_ref key)
{
    write_string(key);
    _output << ':';
}

void ostream_encoder::write_string(string_ref value)
{
    stream_escaped_string(_output, value);
}

}
