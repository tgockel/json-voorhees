/// \file jsonv/serialization/function_adapter.hpp
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

#include "adapter_for.hpp"

#include <expected>
#include <utility>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

template <typename T, typename FDeserialize, typename FToJson>
class function_adapter :
        public adapter_for<T>
{
public:
    template <typename FUDeserialize, typename FUToJson>
    explicit function_adapter(FUDeserialize&& deserialize_, FUToJson&& to_json_) :
            _deserialize(std::forward<FUDeserialize>(deserialize_)),
            _to_json(std::forward<FUToJson>(to_json_))
    { }

protected:
    JSONV_NODISCARD
    virtual std::expected<T, ast_node_type> create(deserialization_context& context, reader& from) const override
    {
        return detail::invoke_deserialize<T>(_deserialize, context, from);
    }

    JSONV_NODISCARD
    virtual value to_json(const serialization_context& context, const T& from) const override
    {
        return to_json_impl(_to_json, context, from);
    }

private:
    template <typename FUToJson>
    static auto to_json_impl(const FUToJson& func, const serialization_context& context, const T& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }

    template <typename FUToJson, typename = void>
    static auto to_json_impl(const FUToJson& func, const serialization_context&, const T& from)
            -> decltype(func(from))
    {
        return func(from);
    }

private:
    FDeserialize _deserialize;
    FToJson  _to_json;
};

/// Create an \c adapter from \a deserialize and \a to_json_, deducing the adapted type from what \a deserialize
/// returns.
template <typename FDeserialize, typename FToJson>
JSONV_NODISCARD
auto make_adapter(FDeserialize deserialize, FToJson to_json_)
    -> function_adapter<detail::deserialize_function_result_t<FDeserialize>, FDeserialize, FToJson>
{
    return function_adapter<detail::deserialize_function_result_t<FDeserialize>, FDeserialize, FToJson>
            (std::move(deserialize), std::move(to_json_));
}

/// \}

}
