/// \file jsonv/serialization/extractor_construction.hpp
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

/// An \c extractor for type \c T that has a constructor that accepts either a \c jsonv::value or a \c jsonv::value and
/// \c extraction_context.
template <typename T>
class extractor_construction :
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
        return extract_impl<T>(context, from, into);
    }

protected:
    template <typename U>
    auto extract_impl(const extraction_context& context,
                      const value&              from,
                      void*                     into
                     ) const
            -> decltype(U(from, context), void())
    {
        new(into) U(from, context);
    }

    template <typename U, typename = void>
    auto extract_impl(const extraction_context&,
                      const value&              from,
                      void*                     into
                     ) const
            -> decltype(U(from), void())
    {
        new(into) U(from);
    }
};

/// \}

}
