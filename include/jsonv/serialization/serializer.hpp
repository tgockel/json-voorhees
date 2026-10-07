/// \file jsonv/serialization/serializer.hpp
///
/// Copyright (c) 2015-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/forward.hpp>
#include <jsonv/writer.hpp>

#include <typeinfo>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// A \c serializer holds the method for converting an arbitrary C++ type into JSON, written token by token into a
/// \c writer.
class JSONV_PUBLIC serializer
{
public:
    virtual ~serializer() noexcept;

    /// Get the run-time type this \c serializer knows how to serialize. Once this \c serializer is registered with a
    /// \c formats, it is not allowed to change.
    JSONV_NODISCARD
    virtual const std::type_info& get_type() const noexcept = 0;

    /// Write the value in the given region of memory \a from into \a to.
    ///
    /// \param context Extra information to help you serialize sub-objects for your type, such as the ability to find
    ///                other \c serializer implementations via \c formats. Serialize each part of your type through
    ///                \c serialization_context::serialize: anything thrown while doing so comes out of it as a
    ///                \c serialization_error naming the type and the position in the document, as
    ///                \c writer::current_path reports it.
    /// \param from The region of memory that represents the C++ value to convert to JSON. The pointer comes as the
    ///             result of a <tt>static_cast&lt;const void*&gt;</tt>, so performing a \c static_cast back to your
    ///             type is okay.
    /// \param to The writer to write into, positioned where the value goes: at the root, as the next element of an open
    ///           array, or as the value of the key just written. Write exactly one value -- a scalar, or a structure
    ///           you open and close -- and nothing else. To refuse, throw: \c to.current_path() is where you are.
    ///
    /// \see serializer_for
    /// \see value_serializer_for
    /// \see adapter_for
    virtual void serialize(const serialization_context& context, const void* from, writer& to) const = 0;
};

/// \}

}
