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

namespace jsonv
{

/// \addtogroup Serialization
/// \{

template <typename T>
class serializer_for :
        public serializer
{
public:
    virtual const std::type_info& get_type() const override
    {
        return typeid(T);
    }

    virtual value to_json(const serialization_context& context,
                          const void*                  from
                         ) const override
    {
        return to_json(context, *static_cast<const T*>(from));
    }

protected:
    virtual value to_json(const serialization_context& context,
                          const T&                     from
                         ) const = 0;
};

/// \}

}
