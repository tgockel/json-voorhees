/** \file jsonv/serialization_optional.hpp
*  Template specialization to support optional<T> serialization.
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

#ifndef __JSONV_SERIALIZATION_OPTIONAL_HPP_INCLUDED__
#define __JSONV_SERIALIZATION_OPTIONAL_HPP_INCLUDED__

#include <jsonv/serialization_util.hpp>

/** \def JSONV_OPTIONAL_TYPE
 *  The type to use for \c jsonv::optional<T>. By default, this is \c jsonv::detail::optional<T>.
 *  
 *  \def JSONV_OPTIONAL_INCLUDE
 *  The file to include to get the implementation for \c jsonv::optional<T>. If you define \c JSONV_OPTIONAL_TYPE, you must
 *  also define this.
 *  
 *  \def JSONV_OPTIONAL_USE_STD
 *  Set this to 1 to use \c std::optional<T> as the backing type for \c jsonv::optional<T>.
 *  
 *  \def JSONV_OPTIONAL_USE_STD_EXPERIMENTAL
 *  Set this to 1 to use \c std::experimental::optional<T> as the backing type for \c jsonv::optional<T>.
 *
 *  \def JSONV_STRING_VIEW_USE_BOOST
 *  Set this to 1 to use \c boost::optinal<T> as the backing type for \c jsonv::optional<T>.
**/
#ifndef JSONV_OPTIONAL_TYPE
#   if defined(JSONV_OPTIONAL_USE_STD) && JSONV_OPTIONAL_USE_STD
#       define JSONV_OPTIONAL_TYPE(T) std::optional<T>
#       define JSONV_OPTIONAL_INCLUDE <optional>
#   elif defined(JSONV_OPTIONAL_USE_STD_EXPERIMENTAL) && JSONV_OPTIONAL_USE_STD_EXPERIMENTAL
#       define JSONV_OPTIONAL_TYPE(T) std::experimental::optional<T>
#       define JSONV_OPTIONAL_INCLUDE <experimental/optional>
#   elif defined(JSONV_OPTIONAL_USE_BOOST) && JSONV_OPTIONAL_USE_BOOST
#       define JSONV_OPTIONAL_TYPE(T) boost::optional<T>
#       define JSONV_OPTIONAL_INCLUDE <boost/optional.hpp>
#   else
#       define JSONV_OPTIONAL_TYPE(T) jsonv::detail::optional<T>
#       define JSONV_OPTIONAL_INCLUDE <jsonv/detail/optional.hpp>
#   endif
#endif

#include JSONV_OPTIONAL_INCLUDE

namespace jsonv
{

template<typename T>
using optional = JSONV_OPTIONAL_TYPE(T);

//specialization for optional<T>
template <typename T>
class container_adapter<optional<T>> :
    public adapter_for<optional<T>>
{
    using element_type = optional<T>;

protected:
    virtual optional<T> create(const extraction_context& context, const value& from) const override
    {
        optional<T> out;
        if (from.is_null()) {
            return out;
        }
        out = context.extract<T>(from);
        return out;
    }

    virtual value to_json(const serialization_context& context, const optional<T>& from) const override
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

