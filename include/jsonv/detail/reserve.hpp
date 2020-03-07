/// \file jsonv/detail/reserve.hpp
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

#include <type_traits>

namespace jsonv::detail
{

template <typename TContainer, typename TCapacity>
void reserve_if_possible_impl(TContainer& container,
                              const TCapacity& capacity,
                              decltype(std::declval<TContainer&>().reserve(std::declval<const TCapacity&>()))*
                             )
{
    container.reserve(capacity);
}

template <typename TContainer, typename TCapacity>
void reserve_if_possible_impl(TContainer&, const TCapacity&, ...)
{ }

/// Call \c reserve on \a container with the given capacity if \a container has a member function for \a reserve. If it
/// does not have a \c reserve function, then this silently does nothing.
template <typename TContainer, typename TCapacity>
void reserve_if_possible(TContainer&      container,
                         const TCapacity& capacity
                        )
{
    reserve_if_possible_impl(container, capacity, nullptr);
}

}
