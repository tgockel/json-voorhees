/// \file jsonv/serialization/context.hpp
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
#include <jsonv/serialization/formats.hpp>
#include <jsonv/version.hpp>

#include <optional>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// Provides extra information to routines used for extraction and serialization.
class JSONV_PUBLIC context
{
public:
    /// Create a new instance using the default \c formats (\c formats::global).
    context();

    /// Create a new instance using the given \a fmt, \a ver and \a userdata.
    explicit context(jsonv::formats                fmt,
                     std::optional<jsonv::version> ver      = std::nullopt,
                     const void*                   userdata = nullptr
                    );

    virtual ~context() noexcept = 0;

    /// Get the \c formats object backing extraction and encoding.
    JSONV_NODISCARD
    const jsonv::formats& formats() const
    {
        return _formats;
    }

    /// Get the version this context was created with. If no version was specified, this is \c std::nullopt -- which
    /// means the caller did not care about versioning, not that the version is \c 0.0.
    JSONV_NODISCARD
    const std::optional<jsonv::version>& version() const
    {
        return _version;
    }

    /// Get a pointer to arbitrary user data.
    JSONV_NODISCARD
    const void* user_data() const
    {
        return _user_data;
    }

private:
    jsonv::formats                _formats;
    std::optional<jsonv::version> _version;
    const void*                   _user_data;
};

/// \}

}
