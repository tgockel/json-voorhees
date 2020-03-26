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
#include <jsonv/serialization/extract.hpp>
#include <jsonv/result.hpp>

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
    /// \see extractor::get_type
    virtual const std::type_info& get_type() const noexcept override
    {
        return typeid(T);
    }

    /// \see extractor::extract
    virtual result<void, void> extract(extraction_context& context, reader& from, void* into) const override
    {
        if (auto res = create(context, from))
        {
            new(into) T(std::move(res).value());
            return ok{};
        }
        else
        {
            return error{};
        }
    }

protected:
    virtual result<T, void> create(extraction_context& context, reader& from) const = 0;
};

/// \}

}
