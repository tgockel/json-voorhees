/// \file jsonv/serialization_optional.hpp
/// Template specialization to support optional<T> serialization. These are usually not needed unless you are writing
/// your own \c extractor or \c serializer.
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Vladimir Venediktov (vvenedict@gmail.com)
#pragma once

#include <jsonv/serialization_util.hpp>
#include <jsonv/optional.hpp>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

template <typename T>
class container_adapter<optional<T>> :
        public adapter_for<optional<T>>
{
    using element_type = optional<T>;

protected:
    virtual optional<T> create(const extraction_context& context, const value& from) const override
    {
        if (from.is_null())
            return nullopt;
        else
            return context.extract<T>(from);
    }

    virtual value to_json(const serialization_context& context, const optional<T>& from) const override
    {
        if (from)
            return context.to_json(*from);
        else
            return null;
    }
};

/// \}

}
