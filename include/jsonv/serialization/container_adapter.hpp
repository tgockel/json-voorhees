/// \file jsonv/serialization/serializer_for.hpp
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
#include <jsonv/serialization.hpp>

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
        public value_adapter_for<TContainer>
{
    using element_type = typename TContainer::value_type;

protected:
    JSONV_NODISCARD
    virtual TContainer create(extraction_context& context, const value& from) const override
    {
        using std::end;

        auto       mark      = context.problems().size();
        TContainer out;
        bool       recovered = false;

        (void) from.as_array(); // get nice error if input is not an array
        for (value::size_type idx = 0U; idx < from.size(); ++idx)
        {
            try
            {
                // Naming the element is what puts `[3]` into a problem raised inside it. The scope lives on this
                // frame and is two stores to push; no `jsonv::path` is built unless a problem is actually
                // recorded, which is why a wholly successful extraction of a large array allocates nothing to
                // track where it is. Reaching the scope through `extract_sub` instead built one per element.
                //
                // It covers the extraction and not the insertion, which is `TContainer`'s code and may be a
                // user's -- the same line `extract_sub` drew by returning before the insert was reached.
                auto extracted = [&] () -> element_type
                                 {
                                     extraction_context::path_scope scope(context, idx);

                                     return context.extract<element_type>(from.at(idx));
                                 }();

                out.insert(end(out), std::move(extracted));
            }
            catch (const extraction_error& ex)
            {
                // The next element starts at a known place, so a bad one does not have to hide every problem after
                // it. Only in `collect_all`, and only while the budget lasts -- otherwise this rethrows and the
                // problems stay in `ex` to be folded on exactly once by the catch above this one.
                if (!context.recover(ex))
                    throw;

                recovered = true;
            }
        }

        // Recovering collected the rest of the problems; it did not make the container valid.
        if (recovered)
            throw extraction_error(context.take_problems_since(mark));

        return out;
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
