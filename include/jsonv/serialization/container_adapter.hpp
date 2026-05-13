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
        public adapter_for<TContainer>
{
    using element_type = typename TContainer::value_type;

protected:
    virtual TContainer create(const extraction_context& context, const value& from) const override
    {
        using std::end;

        TContainer out;
        from.as_array(); // get nice error if input is not an array
        for (value::size_type idx = 0U; idx < from.size(); ++idx)
            out.insert(end(out), context.extract_sub<element_type>(from, idx));
        return out;
    }

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
