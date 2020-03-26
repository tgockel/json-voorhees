/// \file jsonv/serialization/optional_adapter.hpp
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
#include <jsonv/serialization/adapter.hpp>

#include <new>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An adapter for the type \c T. This is a utility class which converts the `void*`s used in the \c extractor and
/// \c serializer interfaces into the more-friendly \c T.
///
/// \see extractor_for
/// \see serializer_for
template <typename T>
class adapter_for :
        public adapter
{
public:
    /// \see adapter::get_type
    /// \see serializer::get_type
    virtual const std::type_info& get_type() const noexcept override
    {
        return typeid(T);
    }

    /// \see adapter::extract
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

    /// \see serializer::to_json
    virtual value to_json(const serialization_context& context,
                          const void*                  from
                         ) const override
    {
        return to_json(context, *static_cast<const T*>(from));
    }

protected:
    /// Create an instance from \a context and \a from.
    ///
    /// \param context Extra information to help you decode sub-objects, such as looking up other \c extractor
    ///                implementations via \c formats.
    /// \param from The JSON \c reader to extract something from.
    ///
    /// \returns A result with \c result_state::ok upon successful extraction from the reader. If extraction is not
    ///          successful, a \c problem should be added to the \a context and an `error<void>` instance should be
    ///          returned.
    virtual result<T, void> create(extraction_context& context, reader& from) const = 0;

    virtual value to_json(const serialization_context& context, const T& from) const = 0;
};

/// \}

}
