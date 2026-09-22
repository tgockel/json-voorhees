/// \file jsonv/serialization.hpp
/// Conversion between C++ types and JSON values.
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
#include <jsonv/serialization/adapter.hpp>
#include <jsonv/serialization/context.hpp>
#include <jsonv/serialization/extract.hpp>
#include <jsonv/serialization/formats.hpp>
#include <jsonv/serialization/serializer.hpp>
#include <jsonv/value.hpp>
#include <jsonv/version.hpp>

#include <optional>
#include <typeinfo>

namespace jsonv
{

/// \addtogroup Serialization
/// \{
/// Serialization components are responsible for conversion between a C++ type and a JSON \c value.

class JSONV_PUBLIC serialization_context :
        public context
{
public:
    /// Create a new instance using the default \c formats (\c formats::global).
    serialization_context();

    /// Create a new instance using the given \a fmt, \a ver and \a userdata.
    explicit serialization_context(jsonv::formats                fmt,
                                   std::optional<jsonv::version> ver      = std::nullopt,
                                   const void*                   userdata = nullptr
                                  );

    virtual ~serialization_context() noexcept;

    /// Convenience function for converting a C++ object into a JSON value.
    ///
    /// \see formats::to_json
    template <typename T>
    JSONV_NODISCARD
    value to_json(const T& from) const
    {
        return to_json(typeid(T), static_cast<const void*>(&from));
    }

    /// Dynamically convert a type into a JSON value.
    ///
    ///  \see formats::to_json
    JSONV_NODISCARD
    value to_json(const std::type_info& type, const void* from) const;
};

/// Encode a JSON \c value from \a from using the provided \a fmts.
template <typename T>
JSONV_NODISCARD
value to_json(const T& from, const formats& fmts)
{
    serialization_context context(fmts);
    return context.to_json(from);
}

/// Encode a JSON \c value from \a from using \c jsonv::formats::global().
template <typename T>
JSONV_NODISCARD
value to_json(const T& from)
{
    serialization_context context;
    return context.to_json(from);
}

/// \}

}
