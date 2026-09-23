/// \file jsonv/detail/reserve.hpp
/// Definition of the \c reserve_if_possible utility.
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

namespace jsonv::detail
{

/// Call \c reserve on \a container with the given \a capacity if it has such a member function; do nothing if it does
/// not.
///
/// This exists because the containers an \c adapter fills are not all node-based or all contiguous: \c std::vector can
/// be sized once from a count known up front, while \c std::list and \c std::set have nothing to size.
template <typename TContainer, typename TCapacity>
void reserve_if_possible(TContainer& container, const TCapacity& capacity)
{
    if constexpr (requires { container.reserve(capacity); })
        container.reserve(capacity);
}

}
