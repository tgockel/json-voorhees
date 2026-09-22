/// \file jsonv/serialization/function_extractor.hpp
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

#include "extractor_for.hpp"

#include <expected>
#include <utility>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An \c extractor which calls a function to perform extraction.
///
/// The function may take any of the shapes \c detail::invoke_extract accepts and may return either a \c T or a
/// \c std::expected<T, ast_node_type>.
template <typename T, typename FExtract>
class function_extractor :
        public extractor_for<T>
{
public:
    template <typename FUExtract>
    explicit function_extractor(FUExtract&& func) :
            _func(std::forward<FUExtract>(func))
    { }

protected:
    JSONV_NODISCARD
    virtual std::expected<T, ast_node_type> create(extraction_context& context, reader& from) const override
    {
        return detail::invoke_extract<T>(_func, context, from);
    }

private:
    FExtract _func;
};

/// Create an \c extractor from \a func, deducing what it extracts from its return type.
template <typename FExtract>
JSONV_NODISCARD
auto make_extractor(FExtract func)
    -> function_extractor<detail::extract_function_result_t<FExtract>, FExtract>
{
    return function_extractor<detail::extract_function_result_t<FExtract>, FExtract>(std::move(func));
}

/// \}

}
