/// \file jsonv/serialization/optional_adapter.hpp
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
#include <jsonv/serialization/adapter.hpp>
#include <jsonv/value.hpp>

#include <new>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An adapter for the type \c T. This is a utility class which converts the `void*`s used in the \c extractor and
/// \c serializer interfaces into the more-friendly \c T.
///
/// \see extractor_for
/// \see serializer_for
template <typename T>
class adapter_for :
        public adapter
{
public:
    /// \see adapter::get_type
    /// \see serializer::get_type
    virtual const std::type_info& get_type() const noexcept override
    {
        return typeid(T);
    }

    /// \see adapter::extract
    virtual std::expected<void, ast_node_type>
    extract(extraction_context& context, reader& from, void* into) const override
    {
        if (auto res = create(context, from))
        {
            new(into) T(std::move(res).value());
            return {};
        }
        else
        {
            return std::unexpected(res.error());
        }
    }

    /// \see serializer::to_json
    virtual value to_json(const serialization_context& context,
                          const void*                  from
                         ) const override
    {
        return to_json(context, *static_cast<const T*>(from));
    }

protected:
    /// Create an instance from \a context and \a from.
    ///
    /// \param context Extra information to help you decode sub-objects, such as looking up other \c extractor
    ///                implementations via \c formats.
    /// \param from The JSON \c reader to extract something from.
    ///
    /// \returns A successful \c std::expected upon successful extraction from the reader. If extraction is not
    ///          successful, a \c problem should be added to the \a context and a \c std::unexpected containing the
    ///          offending \c ast_node_type (or \c ast_node_type::error as a sentinel) should be returned.
    virtual std::expected<T, ast_node_type> create(extraction_context& context, reader& from) const = 0;

    virtual value to_json(const serialization_context& context, const T& from) const = 0;
};

/// A bridge for adapters that were authored against the older `value`-based extraction API. The reader is consumed via
/// \c read_value to build an intermediate JSON tree, then the subclass-supplied `create(context, value)` runs unchanged.
///
/// Use this directly for ports of legacy adapters; for new code prefer \c adapter_for and operate on the \c reader
/// directly.
template <typename T>
class value_adapter_for :
        public adapter_for<T>
{
protected:
    virtual std::expected<T, ast_node_type>
    create(extraction_context& context, reader& from) const final override
    {
        // Let extraction_error propagate; the type-dispatcher in extraction_context::extract folds the inner problem
        // list onto the outer context so the path info from \c throw_extra_keys_extraction_error and friends survives.
        value v = read_value(from);
        return create(context, v);
    }

    virtual T create(extraction_context& context, const value& from) const = 0;
};

/// \}

}
