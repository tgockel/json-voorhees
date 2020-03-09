/// \file jsonv/serialization/function_serializer.hpp
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

#include "serializer_for.hpp"

namespace jsonv
{

/// \addtogroup Serialization
/// \{

template <typename T, typename FToJson>
class function_serializer :
        public serializer_for<T>
{
public:
    template <typename FUToJson>
    explicit function_serializer(FUToJson&& to_json_) :
            _to_json(std::forward<FUToJson>(to_json_))
    { }

protected:

    virtual value to_json(const serialization_context& context, const T& from) const override
    {
        return to_json_impl(_to_json, context, from);
    }

private:
    template <typename FUToJson>
    static auto to_json_impl(const FUToJson& func, const serialization_context& context, const T& from)
            -> decltype(func(context, from))
    {
        return func(context, from);
    }

    template <typename FUToJson, typename = void>
    static auto to_json_impl(const FUToJson& func, const serialization_context&, const T& from)
            -> decltype(func(from))
    {
        return func(from);
    }

private:
    FToJson _to_json;
};

template <typename T, typename FToJson>
function_serializer<T, FToJson> make_serializer(FToJson to_json_)
{
    return function_serializer<T, FToJson>(std::move(to_json_));
}

/// \}

}
