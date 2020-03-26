/// \file jsonv/serialization.hpp
/// Conversion between C++ types and JSON values.
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
#include <jsonv/detail/scope_exit.hpp>
#include <jsonv/path.hpp>
#include <jsonv/serialization/extract.hpp>
#include <jsonv/value.hpp>
#include <jsonv/version.hpp>

#include <memory>
#include <new>
#include <typeinfo>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

namespace jsonv
{

/// \addtogroup Serialization
/// \{
/// Serialization components are responsible for conversion between a C++ type and a JSON \c value.

class extractor;
class value;
class extraction_context;
class serialization_context;

/// A \c serializer holds the method for converting an arbitrary C++ type into a \c value.
class JSONV_PUBLIC serializer
{
public:
    virtual ~serializer() noexcept;

    /** Get the run-time type this \c serialize knows how to encode. Once this \c serializer is registered with a
     *  \c formats, it is not allowed to change.
    **/
    virtual const std::type_info& get_type() const = 0;

    /** Create a \c value \a from the value in the given region of memory.
     *
     *  \param context Extra information to help you encode sub-objects for your type, such as the ability to find other
     *                 \c formats. It also tracks the progression of types in the encoding heirarchy, so any exceptions
     *                 thrown will have \c type information in the error message.
     *  \param from The region of memory that represents the C++ value to convert to JSON. The pointer comes as the
     *              result of a <tt>static_cast&lt;void*&gt;</tt>, so performing a \c static_cast back to your type is
     *              okay.
    **/
    virtual value to_json(const serialization_context& context,
                          const void*                  from
                         ) const = 0;
};

/** Provides extra information to routines used for extraction. **/
class JSONV_PUBLIC context_base
{
public:
    /** Create a new instance using the default \c formats (\c formats::global). **/
    context_base();

    /** Create a new instance using the given \a fmt, \a ver and \a p. **/
    explicit context_base(jsonv::formats        fmt,
                          const jsonv::version& ver      = jsonv::version(1),
                          const void*           userdata = nullptr
                         );

    virtual ~context_base() noexcept = 0;

    /** Get the \c formats object backing extraction and encoding. **/
    const jsonv::formats& formats() const
    {
        return _formats;
    }

    /** Get the version this \c extraction_context was created with. **/
    const jsonv::version version() const
    {
        return _version;
    }

    /** Get a pointer to arbitrary user data. **/
    const void* user_data() const
    {
        return _user_data;
    }

private:
    jsonv::formats _formats;
    jsonv::version _version;
    const void*    _user_data;
};

class JSONV_PUBLIC serialization_context :
        public context_base
{
public:
    /// Create a new instance using the default \c formats (\c formats::global).
    serialization_context();

    /// Create a new instance using the given \a fmt and \a ver.
    explicit serialization_context(jsonv::formats        fmt,
                                   const jsonv::version& ver      = jsonv::version(),
                                   const void*           userdata = nullptr
                                  );

    virtual ~serialization_context() noexcept;

    /// Convenience function for converting a C++ object into a JSON value.
    ///
    /// \see formats::to_json
    template <typename T>
    value to_json(const T& from) const
    {
        return to_json(typeid(T), static_cast<const void*>(&from));
    }

    /// Dynamically convert a type into a JSON value.
    ///
    ///  \see formats::to_json
    value to_json(const std::type_info& type, const void* from) const;
};

/** Encode a JSON \c value from \a from using the provided \a fmts. **/
template <typename T>
value to_json(const T& from, const formats& fmts)
{
    serialization_context context(fmts);
    return context.to_json(from);
}

/** Encode a JSON \c value from \a from using \c jsonv::formats::global(). **/
template <typename T>
value to_json(const T& from)
{
    serialization_context context;
    return context.to_json(from);
}

/// \}

}
