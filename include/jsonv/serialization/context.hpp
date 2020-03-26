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
#include <jsonv/optional.hpp>
#include <jsonv/serialization/formats.hpp>
#include <jsonv/version.hpp>

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

    /// Create a new instance using the given \a formats, optional \a version, and optional \a user_data.
    explicit context(jsonv::formats                  formats,
                     const optional<jsonv::version>& version   = nullopt,
                     const void*                     user_data = nullptr
                    );

    virtual ~context() noexcept = default;

    /// Get the \c formats object backing extraction and encoding.
    const jsonv::formats& formats() const
    {
        return _formats;
    }

    /// Get the version for extraction or serialization. If this context was not created with a \c version, this will be
    /// \c nullopt.
    const optional<jsonv::version>& version() const
    {
        return _version;
    }

    /// Get a pointer to arbitrary user data. This will be \c nullptr if this context was made without user data.
    const void* user_data() const
    {
        return _user_data;
    }

private:
    jsonv::formats           _formats;
    optional<jsonv::version> _version;
    const void*              _user_data;
};

/// \}

}
