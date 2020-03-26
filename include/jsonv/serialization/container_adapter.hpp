/// \file jsonv/serialization/serializer_for.hpp
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/detail/reserve.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/reader.hpp>

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
    virtual result<TContainer, void> create(extraction_context& context, reader& from) const override
    {
        auto first = context.current_as<ast_node::array_begin>(from);
        if (first.is_error())
            return error{};

        TContainer out;
        detail::reserve_if_possible(out, first->element_count());

        bool good = true;
        while (from.next_token())
        {
            auto node = from.current();
            if (node.type() == ast_node_type::array_end)
                break;

            if (auto sub = context.extract<element_type>(from))
            {
                // If we have failed to extract a previous element, don't bother adding it to the output
                if (good)
                {
                    using std::end;
                    out.insert(end(out), std::move(sub).value());
                }
            }
            else
            {
                good = false;
            }
        }

        if (good)
            return ok{ std::move(out) };
        else
            return error{};
    }

    virtual value to_json(const serialization_context& context, const TContainer& from) const override
    {
        value out = array();
        out.reserve(from.size());
        for (const element_type& x : from)
            out.push_back(context.to_json(x));
        return out;
    }
};

/// \}

}
