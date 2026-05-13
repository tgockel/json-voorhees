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
    virtual T create(const extraction_context& context, const value& from) const override
    {
        return create_impl(_func, context, from);
    }

private:
    template <typename FUExtract>
    static auto create_impl(const FUExtract& func, const extraction_context& context, const value& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }

    template <typename FUExtract, typename = void>
    static auto create_impl(const FUExtract& func, const extraction_context&, const value& from)
            -> decltype(func(from))
    {
        return func(from);
    }

private:
    FExtract _func;
};

template <typename FExtract>
auto make_extractor(FExtract func)
    -> function_extractor<decltype(func(std::declval<const extraction_context&>(), std::declval<const value&>())),
                          FExtract
                         >
{
    return function_extractor<decltype(func(std::declval<const extraction_context&>(), std::declval<const value&>())),
                              FExtract
                             >
            (std::move(func));
}

template <typename FExtract, typename = void>
auto make_extractor(FExtract func)
    -> function_extractor<decltype(func(std::declval<const value&>())),
                          FExtract
                         >
{
    return function_extractor<decltype(func(std::declval<const value&>())), FExtract>
            (std::move(func));
}

/// \}

}
