/// \file
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "string_impl.hpp"

#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace jsonv::detail
{

string_impl* string_impl::make(std::string_view text)
{
    // The same bound `std::string::max_size` has, less room for the header and the NUL.
    constexpr std::size_t max_size =
        static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) - sizeof(string_impl) - 1U;
    if (text.size() > max_size)
        throw std::length_error("no string can hold " + std::to_string(text.size()) + " bytes");

    void* storage = ::operator new(sizeof(string_impl) + text.size() + 1U);
    auto* out     = ::new (storage) string_impl(text.size());
    auto* chars   = reinterpret_cast<char*>(out + 1);
    if (!text.empty())
        std::memcpy(chars, text.data(), text.size());
    chars[text.size()] = '\0';
    return out;
}

void string_impl::destroy(string_impl* self) noexcept
{
    self->~string_impl();
    ::operator delete(static_cast<void*>(self));
}

}
