/// \file jsonv/serialization/function_adapter.hpp
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
#include <jsonv/result.hpp>
#include <jsonv/serialization.hpp>

#include <type_traits>

#include "adapter_for.hpp"

namespace jsonv
{
namespace detail
{

template <typename FExtract, typename = void>
struct is_extract_function_with_context_impl : std::false_type
{ };

template <typename FExtract>
struct is_extract_function_with_context_impl
<
    FExtract,
    std::void_t
    <
        decltype(std::declval<FExtract const&>()(std::declval<extraction_context&>(), std::declval<reader&>()))
    >
>
        : std::true_type
{ };

template <typename FExtract, typename = void>
struct is_extract_function_no_context_impl : std::false_type
{ };

template <typename FExtract>
struct is_extract_function_no_context_impl
<
    FExtract,
    std::void_t<decltype(std::declval<FExtract const&>()(std::declval<reader&>()))>
>
        : std::true_type
{ };

/// A function object or function pointer @c FExtract is an _extraction function_ if it can be called with one of:
///
/// - `T (*)(extraction_context&, reader&)` -- an extraction function with context
/// - `T (*)(reader&)` -- an extraction function without context
///
/// If an extraction function matches both, then the version with context is preferred.
template <typename FExtract>
struct is_extract_function :
        std::integral_constant<bool,
                               (  is_extract_function_with_context_impl<FExtract>::value
                               || is_extract_function_no_context_impl<FExtract>::value
                               )
                              >
{ };

template <typename FExtract>
inline constexpr bool is_extract_function_v = is_extract_function<FExtract>::value;

}

/// \addtogroup Serialization
/// \{

template <typename T, typename FExtract, typename FToJson>
class function_adapter :
        public adapter_for<T>
{
    static_assert(detail::is_extract_function_v<FExtract>, "FExtract must be an extraction function");

public:
    template <typename FUExtract, typename FUToJson>
    explicit function_adapter(FUExtract&& extract_, FUToJson&& to_json_) :
            _extract(std::forward<FUExtract>(extract_)),
            _to_json(std::forward<FUToJson>(to_json_))
    { }

protected:
    virtual result<T, void> create(extraction_context& context, reader& from) const override
    {
        auto output = create_impl(detail::is_extract_function_with_context_impl<FExtract>{},
                                  _extract,
                                  context,
                                  from
                                 );

        if constexpr (is_result_v<decltype(output)>)
        {
            return output;
        }
        else
        {
            return ok{ output };
        }
    }

    virtual value to_json(const serialization_context& context, const T& from) const override
    {
        return to_json_impl(_to_json, context, from);
    }

private:
    template <typename FUExtract>
    static auto create_impl(std::true_type, const FUExtract& func, extraction_context& context, reader& from)
    {
        return func(context, from);
    }

    template <typename FUExtract>
    static auto create_impl(std::false_type, const FUExtract& func, extraction_context&, reader& from)
    {
        return func(from);
    }

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
    FExtract _extract;
    FToJson  _to_json;
};

// TODO(#150): These should be deduction guides _and_ have some checking for `result`
template <typename FExtract, typename FToJson>
auto make_adapter(FExtract extract, FToJson to_json_)
    -> function_adapter<decltype(extract(std::declval<extraction_context&>(), std::declval<reader&>())),
                        FExtract,
                        FToJson
                       >
{
    return function_adapter<decltype(extract(std::declval<extraction_context&>(), std::declval<reader&>())),
                            FExtract,
                            FToJson
                           >
            (std::move(extract), std::move(to_json_));
}

template <typename FExtract, typename FToJson, typename = void>
auto make_adapter(FExtract extract, FToJson to_json_)
    -> function_adapter<decltype(extract(std::declval<reader&>())),
                        FExtract,
                        FToJson
                       >
{
    return function_adapter<decltype(extract(std::declval<reader&>())),
                            FExtract,
                            FToJson
                           >
            (std::move(extract), std::move(to_json_));
}

/// \}

}
