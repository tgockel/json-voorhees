/// \file jsonv/serialization/extractor_for.hpp
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

/// An \c extractor for the type \c T.
template <typename T>
class extractor_for :
        public extractor
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

protected:
    virtual T create(const extraction_context& context, const value& from) const = 0;
};


/// \}

}
