/// \file jsonv/detail/match/number.hpp
/// Pattern matching for JSON numeric values.
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <cstddef>

namespace jsonv::detail
{

struct JSONV_LOCAL match_number_result
{
    bool        success;
    bool        decimal;
    std::size_t length;

    explicit constexpr match_number_result(bool success, bool decimal, std::size_t length) :
            success(success),
            decimal(decimal),
            length(length)
    { }

    static inline constexpr match_number_result create_complete(bool decimal, std::size_t length)
    {
        return match_number_result(true, decimal, length);
    }

    static inline constexpr match_number_result create_unmatched(std::size_t length)
    {
        return match_number_result(false, false, length);
    }

    explicit constexpr operator bool() const
    {
        return success;
    }
};

match_number_result JSONV_LOCAL match_number(const char* iter, const char* end);
}
