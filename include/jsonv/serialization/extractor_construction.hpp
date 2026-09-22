/// \file jsonv/serialization/extractor_construction.hpp
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
#include <jsonv/serialization/extract.hpp>

#include <concepts>
#include <expected>
#include <new>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An \c extractor for a type \c T with an extracting constructor.
///
/// Four constructor shapes are accepted, preferred in this order: <tt>T(reader&, extraction_context&)</tt>,
/// <tt>T(reader&)</tt>, <tt>T(const value&, extraction_context&)</tt> and <tt>T(const value&)</tt>. The last two
/// are the shapes which predate extraction running off a \c reader; they are handed a subtree materialised by
/// \c read_value, so they keep working but pay for the tree they were always paying for.
template <typename T>
class extractor_construction :
        public extractor
{
public:
    /// \see extractor::get_type
    JSONV_NODISCARD
    virtual const std::type_info& get_type() const noexcept override
    {
        return typeid(T);
    }

    /// \see extractor::extract
    JSONV_NODISCARD
    virtual std::expected<void, ast_node_type>
    extract(extraction_context& context, reader& from, void* into) const override
    {
        if constexpr (std::constructible_from<T, reader&, extraction_context&>)
        {
            new(into) T(from, context);
        }
        else if constexpr (std::constructible_from<T, reader&>)
        {
            new(into) T(from);
        }
        else if constexpr (std::constructible_from<T, const value&, extraction_context&>)
        {
            detail::borrowed_subtree subtree(context, from);
            new(into) T(subtree.get(), context);
            subtree.commit();
        }
        else
        {
            static_assert(std::constructible_from<T, const value&>,
                          "extractor_construction<T> requires T to be constructible from (reader&, "
                          "extraction_context&), (reader&), (const value&, extraction_context&) or (const value&)"
                         );

            detail::borrowed_subtree subtree(context, from);
            new(into) T(subtree.get());
            subtree.commit();
        }

        // A constructor which fails does so by throwing, and a throwing placement-new constructs nothing, so there is
        // never a half-built object in `into` to clean up here. extraction_context::extract turns the exception into a
        // problem on the context.
        return {};
    }
};

/// \}

}
