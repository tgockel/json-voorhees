/// \file jsonv/detail/match/string.hpp
/// Pattern matching for JSON string values.
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

namespace jsonv
{

class parse_options;

}

namespace jsonv::detail
{

struct JSONV_LOCAL match_string_result
{
    bool        success;
    bool        needs_conversion;
    std::size_t length;

    explicit constexpr match_string_result(bool success, bool needs_conversion, std::size_t length) :
            success(success),
            needs_conversion(needs_conversion),
            length(length)
    { }

    static inline constexpr match_string_result create_complete(bool needs_conversion, std::size_t length)
    {
        return match_string_result(true, needs_conversion, length);
    }

    static inline constexpr match_string_result create_unmatched(std::size_t length)
    {
        return match_string_result(false, false, length);
    }

    explicit constexpr operator bool() const
    {
        return success;
    }
};

match_string_result JSONV_LOCAL match_string(const char* iter, const char* end, const parse_options& options);

}
