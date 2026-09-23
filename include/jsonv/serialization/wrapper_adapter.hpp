/// \file jsonv/serialization/wrapper_adapter.hpp
///
/// Copyright (c) 2015-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/ast.hpp>
#include <jsonv/config.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>

#include <expected>
#include <utility>

#include "adapter_for.hpp"

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An adapter for "wrapper" types.
///
/// \tparam TWrapper A wrapper type with a member \c value_type which represents the underlying wrapped type. This must
///  be explicitly convertible to and from the \c value_type.
template <typename TWrapper>
class wrapper_adapter :
        public adapter_for<TWrapper>
{
    using element_type = typename TWrapper::value_type;

protected:
    JSONV_NODISCARD
    virtual std::expected<TWrapper, ast_node_type> create(extraction_context& context, reader& from) const override
    {
        // Nothing to decide and nothing to position: the wrapped type's extractor reads the same value this one was
        // handed and leaves the cursor where this one owes it.
        // `extraction_context::extract` reports an ordinary failure by returning, so anything which *throws* here
        // does so having already stepped the cursor: a `TWrapper` which rejects what it was handed, or a move of
        // the extracted value. Either way the failure is behind the cursor rather than in front of it, and saying
        // so is what stops whatever recovers from this skipping the following sibling as well.
        try
        {
            auto element = context.extract<element_type>(from);
            if (!element)
                return std::unexpected(element.error());

            return TWrapper(*std::move(element));
        }
        catch (...)
        {
            context.note_value_consumed(from);
            throw;
        }
    }

    JSONV_NODISCARD
    virtual value to_json(const serialization_context& context, const TWrapper& from) const override
    {
        return context.to_json(element_type(from));
    }
};

/// \}

}
