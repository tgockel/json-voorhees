/// \file jsonv/serialization/serialization_error.hpp
/// The exception a failed serialization throws. A header of its own, so that \c formats.hpp can derive
/// \c no_serializer from it without pulling in the rest of the write side.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
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

#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// Exception thrown if there is any problem running \c serialize or \c to_json.
///
/// It says where the failure was found, as the \ref path of the slot the failing \c serializer was writing into, and
/// what was being serialized there, as the demangled \ref type_name. Whatever the serializer threw is kept in
/// \ref nested_ptr, unless the failure was thrown as a \c serialization_error to begin with, as a \c no_serializer is.
///
/// The path is the \c writer::current_path of the writer the failing serializer was handed, so it names a position in
/// the document being written. A serializer on the \c value bridge -- a \c value_serializer_for, a
/// \c value_adapter_for, a function returning a \c value, or a \c polymorphic_adapter subtype registered with
/// \c keyed_subtype_action::check or \c keyed_subtype_action::insert -- serializes its parts through a writer of its
/// own, so a failure inside one of those reports a path from that tree's root rather than from the document's.
class JSONV_PUBLIC serialization_error :
        public std::runtime_error
{
public:
    /// Create a new exception for a failure at \a path while serializing \a type, described by \a message and, if an
    /// exception caused it, carrying that exception as \a cause.
    explicit serialization_error(jsonv::path           path,
                                 const std::type_info& type,
                                 std::string           message,
                                 std::exception_ptr    cause = nullptr
                                );

    /// As above, naming the type by its \c std::type_index.
    explicit serialization_error(jsonv::path            path,
                                 const std::type_index& type,
                                 std::string            message,
                                 std::exception_ptr     cause = nullptr
                                );

    virtual ~serialization_error() noexcept;

    /// The path of the slot the failing serializer was writing into. This is empty for a failure at the root of the
    /// document, and for one which did not happen while writing, such as a \c no_serializer thrown by
    /// \c formats::get_serializer.
    JSONV_NODISCARD
    const jsonv::path& path() const noexcept;

    /// Get an ID for the type which was being serialized.
    JSONV_NODISCARD
    std::type_index type_index() const noexcept;

    /// The demangled name of the type which was being serialized.
    JSONV_NODISCARD
    std::string_view type_name() const noexcept;

    /// Human-readable details about what went wrong, without the position \c what prefixes them with.
    JSONV_NODISCARD
    const std::string& message() const noexcept;

    /// The exception which caused this one, if there was one: what the \c serializer threw. This is \c nullptr when the
    /// failure was thrown as a \c serialization_error in the first place.
    JSONV_NODISCARD
    const std::exception_ptr& nested_ptr() const noexcept;

private:
    explicit serialization_error(jsonv::path            path,
                                 const std::type_index& type,
                                 std::string            type_name,
                                 std::string            message,
                                 std::exception_ptr     cause
                                );

private:
    jsonv::path        _path;
    std::type_index    _type_index;
    std::string        _type_name;
    std::string        _message;
    std::exception_ptr _cause;
};

/// \}

}
