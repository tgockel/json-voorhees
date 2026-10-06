/// \file jsonv/serialization/deserializer_construction.hpp
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

#include <concepts>
#include <expected>
#include <new>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// A \c deserializer for a type \c T with a deserializing constructor.
///
/// Four constructor shapes are accepted, preferred in this order: <tt>T(reader&, deserialization_context&)</tt>,
/// <tt>T(reader&)</tt>, <tt>T(const value&, deserialization_context&)</tt> and <tt>T(const value&)</tt>. The last two
/// are the shapes which predate deserialization running off a \c reader; they are handed a subtree materialised by
/// \c read_value, so they keep working but pay for the tree they were always paying for.
template <typename T>
class deserializer_construction :
        public deserializer
{
public:
    /// \see deserializer::get_type
    JSONV_NODISCARD
    virtual const std::type_info& get_type() const noexcept override
    {
        return typeid(T);
    }

    /// \see deserializer::deserialize
    JSONV_NODISCARD
    virtual std::expected<void, ast_node_type>
    deserialize(deserialization_context& context, reader& from, void* into) const override
    {
        if constexpr (std::constructible_from<T, reader&, deserialization_context&>)
        {
            new(into) T(from, context);
        }
        else if constexpr (std::constructible_from<T, reader&>)
        {
            new(into) T(from);
        }
        else if constexpr (std::constructible_from<T, const value&, deserialization_context&>)
        {
            detail::borrowed_subtree subtree(context, from);
            new(into) T(subtree.get(), context);
            subtree.commit();
        }
        else
        {
            static_assert(std::constructible_from<T, const value&>,
                          "deserializer_construction<T> requires T to be constructible from "
                          "(reader&, deserialization_context&), (reader&), "
                          "(const value&, deserialization_context&) or (const value&)"
                         );

            detail::borrowed_subtree subtree(context, from);
            new(into) T(subtree.get());
            subtree.commit();
        }

        // A constructor which fails does so by throwing, and a throwing placement-new constructs nothing, so there is
        // never a half-built object in `into` to clean up here. deserialization_context::deserialize turns the
        // exception into a problem on the context.
        return {};
    }
};

/// \}

}
