/// \file jsonv/serialization/optional_adapter.hpp
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

#include <new>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An adapter for the type \c T. This is a utility class which converts the `void*`s used in the \c extractor and
/// \c serializer interfaces into the more-friendly \c T.
template <typename T>
class adapter_for :
        public adapter
{
public:
    virtual const std::type_info& get_type() const override
    {
        return typeid(T);
    }

    virtual void extract(const extraction_context& context,
                         const value&              from,
                         void*                     into
                        ) const override
    {
        new(into) T(create(context, from));
    }

    virtual value to_json(const serialization_context& context,
                          const void*                  from
                         ) const override
    {
        return to_json(context, *static_cast<const T*>(from));
    }

protected:
    virtual T create(const extraction_context& context, const value& from) const = 0;

    virtual value to_json(const serialization_context& context, const T& from) const = 0;
};

/// \}

}
