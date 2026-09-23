/// \file jsonv/serialization/optional_adapter.hpp
///
/// Copyright (c) 2017-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/ast.hpp>
#include <jsonv/config.hpp>
#include <jsonv/kind.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>

#include <expected>
#include <utility>

#include "adapter_for.hpp"

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An adapter for optional-like types. This is for convenience of creating an \c adapter for things like
/// \c std::optional or \c boost::optional.
///
/// \tparam TOptional The optional container type. It must have a member type named \c value_type which holds the
///  actual type. It must default-construct to the "none" type and have a single-argument constructor which takes a
///  \c TOptional::value_type. It must support a boolean conversion operator for checking if the value is none and a
///  unary \c operator* for getting the underlying value. Both \c std::optional and \c boost::optional possess all of
///  these properties.
template <typename TOptional>
class optional_adapter :
        public adapter_for<TOptional>
{
    using element_type = typename TOptional::value_type;

protected:
    JSONV_NODISCARD
    virtual std::expected<TOptional, ast_node_type> create(extraction_context& context, reader& from) const override
    {
        // A value-backed reader renders a non-finite `kind::decimal` as `literal_null`, because the token it writes
        // has nowhere to put one -- that is what encoding the value produces, not what the tree holds. Where there is
        // a `value` to ask, its `kind` decides and the rendering does not, which is the same rule the numeric
        // extractors follow for the same reason.
        const value* lent = from.current_value();
        bool         none = lent ? lent->kind() == jsonv::kind::null
                                 : from.current().type() == ast_node_type::literal_null;

        // Everything past here steps the cursor before it builds anything, so a `TOptional` which refuses -- its
        // default constructor for the `null` case, its converting one for the other -- fails with the value behind
        // it rather than in front of it. Saying so is what stops whatever recovers from this skipping the following
        // sibling as well, and is where the failure gets its location from.
        try
        {
            if (none)
            {
                (void) from.next_token();
                return TOptional();
            }

            auto element = context.extract<element_type>(from);
            if (!element)
                return std::unexpected(element.error());

            return TOptional(*std::move(element));
        }
        catch (...)
        {
            context.note_value_consumed(from);
            throw;
        }
    }

    JSONV_NODISCARD
    virtual value to_json(const serialization_context& context, const TOptional& from) const override
    {
        if (from)
            return context.to_json(*from);
        else
            return value();
    }
};

/// \}

}
