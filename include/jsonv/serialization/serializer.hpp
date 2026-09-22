/// \file jsonv/serialization/serializer.hpp
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/forward.hpp>
#include <jsonv/value.hpp>

#include <typeinfo>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// A \c serializer holds the method for converting an arbitrary C++ type into a \c value.
class JSONV_PUBLIC serializer
{
public:
    virtual ~serializer() noexcept;

    /// Get the run-time type this \c serialize knows how to encode. Once this \c serializer is registered with a
    /// \c formats, it is not allowed to change.
    JSONV_NODISCARD
    virtual const std::type_info& get_type() const = 0;

    /// Create a \c value \a from the value in the given region of memory.
    ///
    /// \param context Extra information to help you encode sub-objects for your type, such as the ability to find other
    ///                \c formats. It also tracks the progression of types in the encoding heirarchy, so any exceptions
    ///                thrown will have \c type information in the error message.
    /// \param from The region of memory that represents the C++ value to convert to JSON. The pointer comes as the
    ///             result of a <tt>static_cast&lt;void*&gt;</tt>, so performing a \c static_cast back to your type is
    ///             okay.
    JSONV_NODISCARD
    virtual value to_json(const serialization_context& context,
                          const void*                  from
                         ) const = 0;
};

/// \}

}
