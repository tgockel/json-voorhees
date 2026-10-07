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
#include <jsonv/serialization/serialize.hpp>

#include "adapter_for.hpp"

#include <expected>
#include <utility>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An \c adapter which calls one function to deserialize and another to serialize.
///
/// The functions may take any of the shapes \c detail::invoke_deserialize and \c detail::invoke_serialize accept.
template <typename T, typename FDeserialize, typename FSerialize>
class function_adapter :
        public adapter_for<T>
{
public:
    /// Create an adapter which deserializes by calling \a deserialize_ and serializes by calling \a serialize_.
    template <typename FUDeserialize, typename FUSerialize>
    explicit function_adapter(FUDeserialize&& deserialize_, FUSerialize&& serialize_) :
            _deserialize(std::forward<FUDeserialize>(deserialize_)),
            _serialize(std::forward<FUSerialize>(serialize_))
    { }

protected:
    JSONV_NODISCARD
    virtual std::expected<T, ast_node_type> create(deserialization_context& context, reader& from) const override
    {
        return detail::invoke_deserialize<T>(_deserialize, context, from);
    }

    virtual void serialize(const serialization_context& context, const T& from, writer& to) const override
    {
        detail::invoke_serialize<T>(_serialize, context, from, to);
    }

private:
    FDeserialize _deserialize;
    FSerialize   _serialize;
};

/// Create an \c adapter from \a deserialize_ and \a serialize_, deducing the adapted type from what \a deserialize_
/// returns.
template <typename FDeserialize, typename FSerialize>
JSONV_NODISCARD
auto make_adapter(FDeserialize deserialize_, FSerialize serialize_)
    -> function_adapter<detail::deserialize_function_result_t<FDeserialize>, FDeserialize, FSerialize>
{
    return function_adapter<detail::deserialize_function_result_t<FDeserialize>, FDeserialize, FSerialize>
            (std::move(deserialize_), std::move(serialize_));
}

/// \}

}
