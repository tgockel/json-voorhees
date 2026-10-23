/// \file
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/value.hpp>

#include <string>
#include <utility>

#include "array_impl.hpp"
#include "object_impl.hpp"
#include "string_impl.hpp"

namespace jsonv::detail
{

/// Lets library code which builds a \c value in bulk reach past the public interface: to hand a string over without
/// the copy \c value's constructors make, and to fill an array or object's container directly, without the kind check
/// and the out-of-line call each \c push_back or \c operator[] costs.
struct JSONV_LOCAL value_access final
{
    /// Make a \c kind::string which takes \a text over.
    static value string(std::string text)
    {
        value out;
        out._data.string = new string_impl(std::move(text));
        out._kind        = jsonv::kind::string;
        return out;
    }

    /// The elements of \a array, which must be a \c kind::array.
    static array_impl::array_type& array_values(value& array) noexcept
    {
        return array._data.array->_values;
    }

    /// The members of \a object, which must be a \c kind::object.
    static object_impl::map_type& object_values(value& object) noexcept
    {
        return object._data.object->_values;
    }
};

}
