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
#include <iosfwd>
#include <optional>
#include <string>
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

/// The one place a public entry point serializes a whole document as text: every \c jsonv::serialize overload which
/// writes into a \c std::ostream or returns a \c std::string comes through here.
///
/// The object of the given \a type at \a from is written into \a to through a \c writer of its own, over a compact
/// \c ostream_encoder with \c ostream_encoder::ensure_ascii on, as <tt>writer(std::ostream&)</tt> makes. Once the
/// serializer returns, the writer must hold exactly what a document is: one value, with every structure in it closed.
///
/// \throws serialization_error as \c serialization_context::serialize does; and also when the serializer wrote nothing,
///                             left a structure open or wrote a second value after the first, at
///                             \c writer::current_path naming \a type, with a \c std::logic_error as its
///                             \c serialization_error::nested_ptr. \c serialization_context::to_json throws the same for
///                             the first two; it keeps the second of two values, where text would run them together.
///                             Whatever was written before the failure stays in \a to.
JSONV_PUBLIC void serialize_document(const serialization_context& context,
                                     const std::type_info&        type,
                                     const void*                  from,
                                     std::ostream&                to
                                    );

/// \ref serialize_document into a string of its own, which is handed out. A failure discards what was written.
JSONV_NODISCARD JSONV_PUBLIC std::string serialize_to_string(const serialization_context& context,
                                                             const std::type_info&        type,
                                                             const void*                  from
                                                            );

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

/// Serialize \a from into JSON text using \a fmts (by default \c jsonv::formats::global()).
///
/// The text is one whole document, written compactly with \c ostream_encoder::ensure_ascii on, as \c to_string writes
/// a \c value. It is the text of <tt>to_string(to_json(from, fmts))</tt> with one difference: an object's members are
/// in the order its serializer wrote them -- declaration order, for a type described with the serialization builder
/// DSL -- where a \c value keeps them sorted by key. Nothing is built in between, so a container of a million elements
/// is written without a million-node tree. To pretty-print, or to write well-formed UTF-8 as it is, construct the
/// \c encoder yourself and serialize into a \c writer over it.
///
/// \a from is serialized as the type it is, as for \c to_json: a string literal is an array of \c char, which has no
/// serializer, so pass a \c std::string_view.
///
/// \throws serialization_error if a serializer could not be found or failed, as \c serialization_context::serialize
///                             throws it; or if the serializer for \c T wrote anything but one whole value -- nothing,
///                             a structure left open, or a second value after the first -- since a JSON document is
///                             exactly one value.
template <typename T>
JSONV_NODISCARD
std::string serialize(const T& from, const formats& fmts = formats::global())
{
    return detail::serialize_to_string(serialization_context(fmts), typeid(T), static_cast<const void*>(&from));
}

/// Serialize \a from into JSON text through a \a context the caller built, exactly as the overload above does with one
/// built from \c formats.
///
/// Everything \a context was created with applies: its \c formats, and also what the overload above has no way to be
/// given -- the version and user data its serializers see. The serialization builder DSL's \c since and \c until ask
/// for that version. A \c serialization_context holds nothing about any one serialization, so one may serve any
/// number of them.
///
/// \throws serialization_error for the same reasons as the overload above.
template <typename T>
JSONV_NODISCARD
std::string serialize(const T& from, const serialization_context& context)
{
    return detail::serialize_to_string(context, typeid(T), static_cast<const void*>(&from));
}

/// Serialize \a from into \a to as JSON text using \a fmts (by default \c jsonv::formats::global()).
///
/// What is written is exactly the text the overload returning a \c std::string returns: one whole document, with
/// nothing before or after it. Nothing separates it from a document written into the same stream next, so write a
/// separator yourself if the stream is to be read back.
///
/// \throws serialization_error for the same reasons as the overload returning a \c std::string. What was written
///                             before the failure stays in \a to.
template <typename T>
void serialize(const T& from, std::ostream& to, const formats& fmts = formats::global())
{
    detail::serialize_document(serialization_context(fmts), typeid(T), static_cast<const void*>(&from), to);
}

/// Serialize \a from into \a to as JSON text through a \a context the caller built, exactly as the overload above does
/// with one built from \c formats. Everything \a context was created with applies, as for the overload returning a
/// \c std::string which takes one.
///
/// \throws serialization_error for the same reasons as the overload above. What was written before the failure stays
///                             in \a to.
template <typename T>
void serialize(const T& from, std::ostream& to, const serialization_context& context)
{
    detail::serialize_document(context, typeid(T), static_cast<const void*>(&from), to);
}

/// Serialize \a from into \a to using \a fmts (by default \c jsonv::formats::global()), as one value where the writer
/// is: at the root, as the next element of an open array, or as the value of the key just written.
///
/// This is the mirror of deserializing from a \c reader the caller has positioned. What surrounds the value is the
/// caller's, so open an array once and serialize a million items into it. Unlike the overloads which own their writer,
/// this does not check that \a to ends up holding a whole document.
///
/// \throws serialization_error as \c serialization_context::serialize throws it. What was written before the failure
///                             stays in \a to.
template <typename T>
void serialize(const T& from, writer& to, const formats& fmts = formats::global())
{
    serialization_context context(fmts);
    context.serialize(from, to);
}

/// Serialize \a from into \a to through a \a context the caller built, as one value where the writer is, exactly as the
/// overload above does with one built from \c formats. This is <tt>context.serialize(from, to)</tt>, which is the
/// spelling a \c serializer uses for its parts.
///
/// \throws serialization_error as \c serialization_context::serialize throws it. What was written before the failure
///                             stays in \a to.
template <typename T>
void serialize(const T& from, writer& to, const serialization_context& context)
{
    context.serialize(from, to);
}

/// \}

}
