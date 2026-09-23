/// \file jsonv/serialization/adapter_for.hpp
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

#include <expected>
#include <new>
#include <utility>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An adapter for the type \c T. This is a utility class which converts the `void*`s used in the \c extractor and
/// \c serializer interfaces into the more-friendly \c T.
///
/// \see extractor_for
/// \see serializer_for
/// \see value_adapter_for
template <typename T>
class adapter_for :
        public adapter
{
public:
    /// \see extractor::get_type
    /// \see serializer::get_type
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
        if (auto created = create(context, from))
        {
            try
            {
                new(into) T(std::move(created).value());
            }
            catch (...)
            {
                // `create` succeeded, so the cursor is already one past the value this was built from. Moving it
                // into place is the last thing which can fail, and `T` is the caller's type; a loop recovering from
                // that failure must not step over the value a second time.
                context.note_value_consumed(from);
                throw;
            }
            return {};
        }
        else
        {
            return std::unexpected(created.error());
        }
    }

    /// \see serializer::to_json
    JSONV_NODISCARD
    virtual value to_json(const serialization_context& context,
                          const void*                  from
                         ) const override
    {
        return to_json(context, *static_cast<const T*>(from));
    }

protected:
    /// Create an instance of \c T by reading \a from.
    ///
    /// \param context Extra information to help you decode sub-objects, such as looking up other \c extractor
    ///                implementations via \c formats. Record any problem you encounter with
    ///                \c extraction_context::problem.
    /// \param from The JSON \c reader to extract from. On a successful return it should sit one position past the
    ///             value which was read, as \c reader::next_value would have left it.
    ///
    /// \returns The created instance; otherwise a \c std::unexpected carrying the \c ast_node_type actually found
    ///          when the failure was a type mismatch, or \c ast_node_type::error otherwise.
    JSONV_NODISCARD
    virtual std::expected<T, ast_node_type> create(extraction_context& context, reader& from) const = 0;

    JSONV_NODISCARD
    virtual value to_json(const serialization_context& context, const T& from) const = 0;
};

/// A base for adapters written against the older \c value -based extraction interface.
///
/// The subtree under the reader is materialised with \c read_value and handed to the subclass, whose \c create runs
/// unchanged. This costs the whole subtree in memory, which is exactly what extracting off a \c reader is meant to
/// avoid -- so it is a stepping stone for ports, not a destination. New adapters should derive from \c adapter_for and
/// walk the \c reader.
template <typename T>
class value_adapter_for :
        public adapter_for<T>
{
protected:
    JSONV_NODISCARD
    virtual std::expected<T, ast_node_type>
    create(extraction_context& context, reader& from) const final override
    {
        // An extraction_error thrown by the subclass is deliberately left to propagate: extraction_context::extract
        // folds its problem list onto the context, which is how the path and message an older adapter throws with
        // survive into the pipeline's std::expected channel.
        detail::borrowed_subtree subtree(context, from);
        T                        out = create(context, subtree.get());

        // Only once the older body is through: a throw out of it leaves the cursor on the value which failed, so the
        // problem names that rather than whatever follows it.
        subtree.commit();
        return out;
    }

    /// Create an instance of \c T from the materialised \a from.
    ///
    /// \throws extraction_error if \a from cannot be converted to a \c T.
    JSONV_NODISCARD
    virtual T create(extraction_context& context, const value& from) const = 0;
};

/// \}

}
