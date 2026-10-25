/// \file
/// Internal-only header for array-related values.
///
/// Copyright (c) 2014-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/value.hpp>

#include "detail/counted_block.hpp"

namespace jsonv::detail
{

/// The elements of an array: what \c value_storage::array points at, or null for an empty array with no room.
class JSONV_LOCAL array_impl final :
        public counted_block<value, array_impl>
{
private:
    explicit array_impl(size_type capacity) noexcept :
            counted_block(capacity)
    { }

    friend class counted_block<value, array_impl>;
};

}
