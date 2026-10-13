/// \file jsonv/detail/integer_sum.hpp
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <cstdint>
#include <limits>

namespace jsonv::detail
{

/// Does `a + b` fit in an `std::int64_t`? Signed overflow is undefined behavior, so this has to be asked before the
/// addition rather than after it. It is plain comparisons because the compiler builtins for this are not portable
/// (`__builtin_add_overflow` is not on MSVC).
///
/// Neither subtraction can overflow itself: with `b > 0`, `max - b` is at least 0, and with `b <= 0`, `min - b` is
/// between `min` and 0.
constexpr bool integer_sum_fits(std::int64_t a, std::int64_t b) noexcept
{
    return b > 0 ? a <= std::numeric_limits<std::int64_t>::max() - b
                 : a >= std::numeric_limits<std::int64_t>::min() - b;
}

}
