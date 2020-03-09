/// \file jsonv/serialization/optional_adapter.hpp
///
/// Copyright (c) 2017-2020 by Travis Gockel. All rights reserved.
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
    virtual TOptional create(const extraction_context& context, const value& from) const override
    {
        if (from.is_null())
            return TOptional();
        else
            return TOptional(context.extract<element_type>(from));
    }

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
