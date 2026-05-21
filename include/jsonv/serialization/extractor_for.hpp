/// \file jsonv/serialization/extractor_for.hpp
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
#include <jsonv/serialization/extract.hpp>
#include <expected>

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
    virtual std::expected<void, ast_node_type>
    extract(extraction_context& context, reader& from, void* into) const override
    {
        if (auto res = create(context, from))
        {
            new(into) T(std::move(res).value());
            return {};
        }
        else
        {
            return std::unexpected(res.error());
        }
    }

protected:
    virtual std::expected<T, ast_node_type> create(extraction_context& context, reader& from) const = 0;
};

/// \}

}
