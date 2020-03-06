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

#include <cstdint>
#include <iosfwd>

namespace jsonv
{

/// Represents a version used to extract and encode JSON objects from C++ classes. This is useful for API versioning: if
/// you need certain fields to be serialized only after a certain version or only before a different one.
struct JSONV_PUBLIC version
{
public:
    using version_element = std::uint32_t;

public:
    /// Initialize an instance with the given \a major and \a minor version info.
    constexpr version(version_element major = 0, version_element minor = 0) noexcept :
            major{major},
            minor{minor}
    { }

    /// Check if this version is an "empty" value -- meaning \c major and \c minor are both \c 0.
    constexpr bool empty() const noexcept
    {
        return major == 0 && minor == 0;
    }

    /// Convert this instance into a \c uint64_t. The \c major version will be in the higher-order bits, while \c minor
    /// will be in the lower-order bits.
    explicit constexpr operator std::uint64_t() const
    {
        return static_cast<std::uint64_t>(major) << 32
             | static_cast<std::uint64_t>(minor) <<  0;
    }

    /// Test for equality with \a other.
    constexpr bool operator==(const version& other) const
    {
        return static_cast<std::uint64_t>(*this) == static_cast<std::uint64_t>(other);
    }

    /// Test for inequality with \a other.
    constexpr bool operator!=(const version& other) const
    {
        return static_cast<std::uint64_t>(*this) != static_cast<std::uint64_t>(other);
    }

    /// Check that this version is less than \a other. The comparison is done lexicographically.
    constexpr bool operator<(const version& other) const
    {
        return static_cast<std::uint64_t>(*this) < static_cast<std::uint64_t>(other);
    }

    /// Check that this version is less than or equal to \a other. The comparison is done lexicographically.
    constexpr bool operator<=(const version& other) const
    {
        return static_cast<std::uint64_t>(*this) <= static_cast<std::uint64_t>(other);
    }

    /// Check that this version is greater than \a other. The comparison is done lexicographically.
    constexpr bool operator>(const version& other) const
    {
        return static_cast<std::uint64_t>(*this) > static_cast<std::uint64_t>(other);
    }

    /// Check that this version is greater than or equal to \a other. The comparison is done lexicographically.
    constexpr bool operator>=(const version& other) const
    {
        return static_cast<std::uint64_t>(*this) >= static_cast<std::uint64_t>(other);
    }

public:
    version_element major;
    version_element minor;
};

JSONV_PUBLIC std::ostream& operator<<(std::ostream&, const version&);

}
