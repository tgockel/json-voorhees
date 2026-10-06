/// \file jsonv/serialization/function_deserializer.hpp
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
#include <jsonv/serialization/deserialize.hpp>

#include "deserializer_for.hpp"

#include <concepts>
#include <expected>
#include <type_traits>
#include <utility>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// A \c deserializer which calls a function to perform deserialization.
///
/// The function may take any of the shapes \c detail::invoke_deserialize accepts and may return either a \c T or a
/// \c std::expected<T, ast_node_type>.
template <typename T, typename FDeserialize>
class function_deserializer :
        public deserializer_for<T>
{
public:
    // Constrained so that copying from a non-const lvalue reaches the copy constructor, which this would otherwise
    // beat as the better match.
    template <typename FUDeserialize>
        requires (!std::same_as<std::remove_cvref_t<FUDeserialize>, function_deserializer>)
    explicit function_deserializer(FUDeserialize&& func) :
            _func(std::forward<FUDeserialize>(func))
    { }

protected:
    JSONV_NODISCARD
    virtual std::expected<T, ast_node_type> create(deserialization_context& context, reader& from) const override
    {
        return detail::invoke_deserialize<T>(_func, context, from);
    }

private:
    FDeserialize _func;
};

/// Create a \c deserializer from \a func, deducing what it deserializes from its return type.
template <typename FDeserialize>
JSONV_NODISCARD
auto make_deserializer(FDeserialize func)
    -> function_deserializer<detail::deserialize_function_result_t<FDeserialize>, FDeserialize>
{
    return function_deserializer<detail::deserialize_function_result_t<FDeserialize>, FDeserialize>(std::move(func));
}

/// \}

}
