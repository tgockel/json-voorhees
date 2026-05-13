/// \file jsonv/serialization/wrapper_adapter.hpp
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
    virtual TWrapper create(const extraction_context& context, const value& from) const override
    {
        return TWrapper(context.extract<element_type>(from));
    }

    virtual value to_json(const serialization_context& context, const TWrapper& from) const override
    {
        return context.to_json(element_type(from));
    }
};

/// \}

}
