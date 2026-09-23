/// \file jsonv/serialization/container_adapter.hpp
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
#include <jsonv/detail/reserve.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>

#include <cstddef>
#include <expected>
#include <iterator>
#include <utility>

#include "adapter_for.hpp"

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An adapter for container types. This is for convenience of creating an \c adapter for things like \c std::vector,
/// \c std::set and such.
///
/// \tparam TContainer is the container to create and encode. It must have a member type \c value_type, support
///                    iteration and an \c insert operation.
template <typename TContainer>
class container_adapter :
        public adapter_for<TContainer>
{
    using element_type = typename TContainer::value_type;

protected:
    JSONV_NODISCARD
    virtual std::expected<TContainer, ast_node_type> create(extraction_context& context, reader& from) const override
    {
        using std::end;

        auto opened = context.current_as<ast_node::array_begin>(from);
        if (!opened)
            return std::unexpected(opened.error());

        TContainer out;

        // The opening token carries how many elements the array holds, so the container is sized once rather than
        // grown. A failed parse leaves that count determinate and bounded by what is actually on the tape, which is
        // what keeps a malformed document from driving an arbitrary reservation.
        detail::reserve_if_possible(out, opened->element_count());

        bool recovered = false;
        bool closed    = false;

        // Step off the `[` and onto the first element, or onto the `]` of an empty array. The loop never advances
        // itself: extracting an element leaves the cursor one past it, which is what every extractor owes its caller.
        (void) from.next_token();

        try
        {
            for (std::size_t idx = 0U; from.good(); ++idx)
            {
                // A parse which failed part-way through an array still hands back a usable tape; it just ends with
                // the document's end and an `error` describing what cut it short, where the rest of the elements
                // should have been. Saying the array never closed is more use than letting the extraction below
                // report it as a mismatch against a node type no element can have, and there is nothing after it to
                // recover into.
                if (auto type = from.current().type();
                    type == ast_node_type::document_end || type == ast_node_type::error)
                {
                    return context.problem(context.problem_path(from), "Unterminated array");
                }

                if (from.current().type() == ast_node_type::array_end)
                {
                    (void) from.next_token();
                    closed = true;

                    // Moving `out` into the result is the last thing which can fail, and `TContainer` may be a
                    // user's; `closed` is what tells the handler below that there is nothing left to walk.
                    if (!recovered)
                        return out;

                    // Recovering collected the rest of the problems; it did not make the container valid. The whole
                    // array has been read either way, so the cursor lands in the same place whether this succeeds
                    // or fails -- which only works because the failure says the value is behind it.
                    context.note_value_consumed(from);
                    return std::unexpected(ast_node_type::error);
                }

                // Naming the element is what puts `[3]` into a problem raised inside it. The scope lives on this
                // frame and is two stores to push; no `jsonv::path` is built unless a problem is actually recorded,
                // which is why a wholly successful extraction of a large array allocates nothing to track where it
                // is.
                //
                // It covers the extraction and not the insertion, which is `TContainer`'s code and may be a user's.
                auto element = [&] () -> std::expected<element_type, ast_node_type>
                               {
                                   extraction_context::path_scope scope(context, idx);

                                   return context.extract<element_type>(from);
                               }();

                if (!element)
                {
                    // The next element starts at a known place, so a bad one does not have to hide every problem
                    // after it. Only in `collect_all`, and only while the budget lasts -- otherwise the failure is
                    // reported as it stands, with the cursor still naming the element which caused it.
                    if (!context.recover())
                        return std::unexpected(element.error());

                    recovered = true;
                    context.skip_failed_value(from);
                    continue;
                }

                out.insert(end(out), *std::move(element));
            }
        }
        catch (...)
        {
            // Something left the walk -- inserting into `TContainer`, which is often a user's code, or moving an
            // element into place. Unless the array was already closed, the cursor is somewhere inside it: a
            // position only this adapter can make sense of, so finish the walk before letting the failure out.
            //
            // Stepping over whole child values is what finds this array's own end. `reader::next_structure` cannot:
            // on a child which is itself a structure it leaves *that* child, landing back inside this array, and on
            // a scalar child it leaves this array without consuming the `]` consistently. `reader::next_value`
            // crosses a child of either shape, so the only closing token this loop can stop on is its own.
            if (!closed)
            {
                while (from.good())
                {
                    auto type = from.current().type();
                    if (type == ast_node_type::array_end)
                    {
                        (void) from.next_token();
                        break;
                    }
                    else if (type == ast_node_type::document_end || type == ast_node_type::error)
                    {
                        // A truncated tape has no `]` to find. Stopping here leaves the cursor on the end of the
                        // document, which the loop above reports as an unterminated array anyway.
                        break;
                    }

                    (void) from.next_value();
                }
            }

            context.note_value_consumed(from);
            throw;
        }

        // Only reachable by recovering off the end of a truncated tape; the check at the top of the loop is what an
        // unterminated array normally arrives as.
        return context.problem(context.problem_path(from), "Unterminated array");
    }

    JSONV_NODISCARD
    virtual value to_json(const serialization_context& context, const TContainer& from) const override
    {
        value out = array();
        for (const element_type& x : from)
            out.push_back(context.to_json(x));
        return out;
    }
};

/// \}

}
