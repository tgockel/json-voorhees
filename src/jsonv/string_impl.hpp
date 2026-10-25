/// \file
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <cstddef>
#include <string_view>

namespace jsonv::detail
{

/// The text of a string too long for its \c value to hold in itself: the length, then the text and a NUL after it, in
/// one allocation.
class JSONV_LOCAL string_impl final
{
public:
    string_impl(const string_impl&)            = delete;
    string_impl& operator=(const string_impl&) = delete;

    /// A block holding a copy of \a text.
    ///
    /// \throws std::length_error if no block could hold \a text.
    static string_impl* make(std::string_view text);

    /// Free \a self.
    static void destroy(string_impl* self) noexcept;

    std::string_view view() const noexcept
    {
        return std::string_view(reinterpret_cast<const char*>(this + 1), _size);
    }

private:
    explicit string_impl(std::size_t size) noexcept :
            _size(size)
    { }

    ~string_impl() noexcept = default;

private:
    std::size_t _size;
};

}
