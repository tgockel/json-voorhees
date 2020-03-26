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

#include "extractor_for.hpp"

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An \c extractor which calls a function to perform extraction.
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
    virtual result<T, void> create(extraction_context& context, reader& from) const override
    {
        using create_result_type = decltype(create_impl(_func, context, from));

        if constexpr (is_result_v<create_result_type>)
        {
            return create_impl(_func, context, from);
        }
        else
        {
            return ok{ create_impl(_func, context, from) };
        }
    }

private:
    template <typename FUExtract>
    static auto create_impl(const FUExtract& func, extraction_context& context, reader& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }

    template <typename FUExtract, typename = void>
    static auto create_impl(const FUExtract& func, extraction_context&, reader& from)
            -> decltype(func(from))
    {
        return func(from);
    }

private:
    FExtract _func;
};

// TODO(#150): These should be deduction guides _and_ have some checking for `result`
template <typename FExtract>
auto make_extractor(FExtract func)
    -> function_extractor<decltype(func(std::declval<extraction_context&>(), std::declval<reader&>())),
                          FExtract
                         >
{
    return function_extractor<decltype(func(std::declval<extraction_context&>(), std::declval<reader&>())),
                              FExtract
                             >
            (std::move(func));
}

template <typename FExtract, typename = void>
auto make_extractor(FExtract func)
    -> function_extractor<decltype(func(std::declval<reader&>())),
                          FExtract
                         >
{
    return function_extractor<decltype(func(std::declval<reader&>())), FExtract>
            (std::move(func));
}

/// \}

}
