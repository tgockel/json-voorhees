/** \file jsonv/serialization_boost.hpp
*  Specialization types for serialization of boost classes and containers.
*  These are usually not needed unless you are writing your own
*  \c extractor or \c serializer.
*
*  Copyright (c) 2015 by Travis Gockel. All rights reserved.
*
*  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
*  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
*  version.
*
*  \author Vladimir Venediktov (vvenedict@gmail.com)
**/

#ifndef __JSONV_SERIALIZATION_BOOST_HPP_INCLUDED__
#define __JSONV_SERIALIZATION_BOOST_HPP_INCLUDED__

#include <jsonv/serialization_util.hpp>
#include <boost/optional.hpp>

namespace jsonv
{

//specialization for boost::optional
template <typename T>
class container_adapter<boost::optional<T>> :
    public adapter_for<boost::optional<T>>
{
    using element_type = boost::optional<T>;

protected:
    virtual boost::optional<T> create(const extraction_context& context, const value& from) const override
    {
        boost::optional<T> out;
        if (from.is_null()) {
            return out;
        }
        out = context.extract<T>(from);
        return out;
    }

    virtual value to_json(const serialization_context& context, const boost::optional<T>& from) const override
    {
        value out;
        if (from) {
            out = context.to_json(*from);
        }
        return out;
    }
};

}

#endif
