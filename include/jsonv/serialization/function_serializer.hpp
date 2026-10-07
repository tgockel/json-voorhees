/// \file jsonv/serialization/function_serializer.hpp
///
/// Copyright (c) 2015-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/serialization/serialize.hpp>

#include "serializer_for.hpp"

#include <concepts>
#include <type_traits>
#include <utility>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// A \c serializer which calls a function to perform serialization.
///
/// The function may take any of the shapes \c detail::invoke_serialize accepts: it may write into the \c writer itself
/// or return a \c value to be written.
template <typename T, typename FSerialize>
class function_serializer :
        public serializer_for<T>
{
public:
    /// Create a serializer which calls \a func.
    // Constrained so that copying from a non-const lvalue reaches the copy constructor, which this would otherwise
    // beat as the better match.
    template <typename FUSerialize>
        requires (!std::same_as<std::remove_cvref_t<FUSerialize>, function_serializer>)
    explicit function_serializer(FUSerialize&& func) :
            _func(std::forward<FUSerialize>(func))
    { }

protected:
    virtual void serialize(const serialization_context& context, const T& from, writer& to) const override
    {
        detail::invoke_serialize<T>(_func, context, from, to);
    }

private:
    FSerialize _func;
};

/// Create a \c serializer for \c T from \a func, which may take any of the shapes \c detail::invoke_serialize accepts.
template <typename T, typename FSerialize>
JSONV_NODISCARD
function_serializer<T, FSerialize> make_serializer(FSerialize func)
{
    return function_serializer<T, FSerialize>(std::move(func));
}

/// \}

}
