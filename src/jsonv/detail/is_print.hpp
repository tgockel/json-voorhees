/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/config.hpp>

namespace jsonv::detail
{

/// Like `std::isprint`, but significantly faster. It assumes \a c is in the ASCII space.
JSONV_ALWAYS_INLINE inline constexpr bool is_print(char c)
{
    return c >= '\x20' && c != '\x7f';
}

}
