/// \file jsonv/serialization/serializer_for.hpp
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
#include <jsonv/serialization/serialize.hpp>
#include <jsonv/serialization/serializer.hpp>
#include <jsonv/value.hpp>

#include <typeinfo>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// A \c serializer for type \c T. This is a utility class which converts the `const void*` used in the \c serializer
/// interface into the more-friendly \c T.
///
/// \see adapter_for
/// \see value_serializer_for
template <typename T>
class serializer_for :
        public serializer
{
public:
    /// \see serializer::get_type
    JSONV_NODISCARD
    virtual const std::type_info& get_type() const noexcept override
    {
        return typeid(T);
    }

    /// \see serializer::serialize
    virtual void serialize(const serialization_context& context, const void* from, writer& to) const override
    {
        serialize(context, *static_cast<const T*>(from), to);
    }

protected:
    /// Write \a from into \a to.
    ///
    /// \see serializer::serialize
    virtual void serialize(const serialization_context& context, const T& from, writer& to) const = 0;
};

/// A base for serializers written against the older \c value -based interface.
///
/// The \c value the subclass's \c to_json builds is written whole into the writer with \c writer::write. This costs the
/// tree the \c writer exists to avoid -- so it is a stepping stone for ports, not a destination. New serializers should
/// derive from \c serializer_for and write into the \c writer.
template <typename T>
class value_serializer_for :
        public serializer_for<T>
{
protected:
    virtual void serialize(const serialization_context& context, const T& from, writer& to) const final override
    {
        to.write(to_json(context, from));
    }

    /// Convert \a from into a \c value.
    ///
    /// \param context Extra information to help you encode sub-objects for your type, such as the ability to find other
    ///                \c formats.
    /// \param from The value to convert.
    JSONV_NODISCARD
    virtual value to_json(const serialization_context& context, const T& from) const = 0;
};

/// \}

}
