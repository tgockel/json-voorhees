/// \file
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

#include <algorithm>
#include <limits>
#include <type_traits>

namespace jsonv::detail
{

/// Clamp \a from to the limits of \c TTo. This is similar to calling \c std::clamp with the numeric limits of \c TTo,
/// but covers cases where the limits of \c TFrom are not representable in the target type. Clamping from signed to
/// unsigned is well-defined with \c 0 as the lower bound.
///
/// \tparam TTo The destination type to cast to. This must be an integral type.
/// \tparam TFrom The source type to cast from. The must be an integral type.
template <typename TTo, typename TFrom>
constexpr TTo clamp_cast(const TFrom& from)
{
    static_assert(std::is_integral_v<TTo>);
    static_assert(std::is_integral_v<TFrom>);

    using to_limits  = std::numeric_limits<TTo>;
    using wider_type = std::conditional_t<(sizeof(TFrom) < sizeof(TTo)), TTo, TFrom>;

    // Same type: just return
    if constexpr (std::is_same_v<TTo, TFrom>)
    {
        return from;
    }
    // Unsigned source: clamp to upper bound of destination
    else if constexpr (std::is_unsigned_v<TFrom>)
    {
        return TTo(std::min(wider_type(from), wider_type(to_limits::max())));
    }
    // Unsigned destination with signed source
    else if constexpr (std::is_unsigned_v<TTo>)
    {
        if (from < TFrom(0))
        {
            return TTo(0);
        }
        else
        {
            return TTo(std::min(wider_type(from), wider_type(to_limits::max())));
        }
    }
    // Signed to signed
    else
    {
        return TTo(std::clamp(wider_type(from), wider_type(to_limits::min()), wider_type(to_limits::max())));
    }
}

}
