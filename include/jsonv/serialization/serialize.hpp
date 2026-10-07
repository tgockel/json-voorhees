/// \file jsonv/serialization/serialize.hpp
/// Serialization of C++ types into a JSON token stream.
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
#include <jsonv/forward.hpp>
#include <jsonv/path.hpp>
#include <jsonv/serialization/context.hpp>
#include <jsonv/serialization/serialization_error.hpp>
#include <jsonv/value.hpp>
#include <jsonv/version.hpp>
#include <jsonv/writer.hpp>

#include <concepts>
#include <optional>
#include <typeinfo>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// Provides extra information to routines used for serialization: the \c formats to find other serializers in, and
/// the \c version and user data the caller asked for.
///
/// Unlike a \c deserialization_context, this is immutable, and one instance can serve any number of serializations at
/// once. Nothing is recorded on it: a serializer's position in the document is its \c writer::current_path, and a
/// failure is reported by throwing a \c serialization_error.
class JSONV_PUBLIC serialization_context :
        public context
{
public:
    /// Create a new instance using the default \c formats (\c formats::global).
    serialization_context();

    /// Create a new instance using the given \a fmt, \a ver and \a userdata.
    explicit serialization_context(jsonv::formats                fmt,
                                   std::optional<jsonv::version> ver      = std::nullopt,
                                   const void*                   userdata = nullptr
                                  );

    virtual ~serialization_context() noexcept;

    /// Write \a from into \a to using the \c formats associated with this context.
    ///
    /// This is the positioned primitive a composite calls for each of its parts: it writes one value where the writer
    /// is -- at the root, as the next element of an open array, or as the value of the key just written -- and nothing
    /// else.
    ///
    /// \throws serialization_error if the \c serializer for \c T cannot be found or throws. The writer may have been
    ///                             given a prefix of the value by then.
    template <typename T>
    void serialize(const T& from, writer& to) const
    {
        serialize(typeid(T), static_cast<const void*>(&from), to);
    }

    /// Write the object of the given \a type at \a from into \a to using the \c formats associated with this context.
    /// This is what the overload above calls with the \c T it was asked for.
    ///
    /// A \c serialization_error thrown by the \c serializer -- a \c no_serializer included -- is rethrown as it is,
    /// since it already says where it is and what was being serialized there. Anything else thrown is wrapped once in a
    /// \c serialization_error at \c to.current_path() naming \a type, with the original exception as its
    /// \c serialization_error::nested_ptr.
    ///
    /// \throws serialization_error as above. The writer may have been given a prefix of the value by then: an entry
    ///                             point which owns its sink discards it, and one writing into a caller's writer leaves
    ///                             what was written where it is.
    void serialize(const std::type_info& type, const void* from, writer& to) const;

    /// Convenience function for converting a C++ object into a JSON value.
    ///
    /// This runs the same pipeline as \c serialize, writing into a \c value_encoder and handing out what it built. It
    /// is how a serializer written against the older \c value -based interface serializes its parts, and what the free
    /// \c jsonv::to_json runs.
    ///
    /// \throws serialization_error as \c serialize does, and also when the serializer wrote nothing or left a
    ///                             structure open, since a JSON document is never empty.
    template <typename T>
    JSONV_NODISCARD
    value to_json(const T& from) const
    {
        return to_json(typeid(T), static_cast<const void*>(&from));
    }

    /// Dynamically convert a type into a JSON value. This is what the overload above calls with the \c T it was asked
    /// for.
    JSONV_NODISCARD
    value to_json(const std::type_info& type, const void* from) const;
};

namespace detail
{

/// Call \a func as a serialization function, writing what it produces into \a to.
///
/// Four call shapes are accepted, tried in this order: <tt>(context, from, writer)</tt>, <tt>(from, writer)</tt>,
/// <tt>(context, from)</tt> and <tt>(from)</tt>. The first two write into the writer themselves. The last two are the
/// interface functions were written against before serialization ran through a \c writer: they return a \c value, or
/// anything one can be built from, which is written whole.
///
/// Each shape is tested with \c std::invocable, which asks whether the call is well-formed and nothing more. A generic
/// lambda is invocable with any arguments its parameter list admits, so one taking <tt>(const auto&, const auto&)</tt>
/// is tried as <tt>(from, writer)</tt> first and fails outright if its body only makes sense for a context, as the
/// shapes of \c invoke_deserialize do. Name the parameter types.
template <typename T, typename FSerialize>
void invoke_serialize(const FSerialize& func, const serialization_context& context, const T& from, writer& to)
{
    if constexpr (std::invocable<const FSerialize&, const serialization_context&, const T&, writer&>)
    {
        func(context, from, to);
    }
    else if constexpr (std::invocable<const FSerialize&, const T&, writer&>)
    {
        func(from, to);
    }
    else if constexpr (std::invocable<const FSerialize&, const serialization_context&, const T&>)
    {
        to.write(func(context, from));
    }
    else
    {
        static_assert(std::invocable<const FSerialize&, const T&>,
                      "A serialization function must be callable as (const serialization_context&, const T&, writer&), "
                      "(const T&, writer&), (const serialization_context&, const T&) or (const T&)"
                     );

        to.write(func(from));
    }
}

}

/// Encode a JSON \c value from \a from using the provided \a fmts.
template <typename T>
JSONV_NODISCARD
value to_json(const T& from, const formats& fmts)
{
    serialization_context context(fmts);
    return context.to_json(from);
}

/// Encode a JSON \c value from \a from using \c jsonv::formats::global().
template <typename T>
JSONV_NODISCARD
value to_json(const T& from)
{
    serialization_context context;
    return context.to_json(from);
}

/// \}

}
