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

    /// Make an empty \c kind::array with room for \a capacity elements.
    static value array(value::size_type capacity)
    {
        value out;
        out._data.array = array_impl::allocate(capacity);
        out._kind       = jsonv::kind::array;
        return out;
    }

    /// Add \a item to the end of \a array, which must be a \c kind::array.
    static void array_push_back(value& array, value&& item)
    {
        array_impl::push_back(array._data.array, std::move(item));
    }

    /// The storage of \a object, which must be a \c kind::object -- for an \c object_builder to fill.
    static object_impl& object_storage(value& object) noexcept
    {
        return *object._data.object;
    }
};

}
