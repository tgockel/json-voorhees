/// \file jsonv/serialization/extract.hpp
/// Extraction of C++ types from JSON values.
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
#include <jsonv/serialization/context.hpp>
#include <jsonv/value.hpp>

#include <exception>
#include <new>
#include <optional>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// Exception thrown if there is any problem running \c extract.
class JSONV_PUBLIC extraction_error :
        public std::runtime_error
{
public:
    /// Description of a single problem with extraction.
    class problem
    {
    public:
        /// \{
        /// Create a problem for the given \a path, \a message, and optional \a cause.
        explicit problem(jsonv::path path, std::string message, std::exception_ptr cause) noexcept;
        explicit problem(jsonv::path path, std::string message) noexcept;
        /// \}

        /// Create a problem with a \c message extracted from \a cause.
        ///
        /// \param cause The underlying cause of this problem to extract the message from. If the exception backing
        ///              \a cause is not derived from \c std::exception, a message about unknown exception will be used
        ///              instead.
        explicit problem(jsonv::path path, std::exception_ptr cause) noexcept;

        /// The path this problem was encountered at.
        JSONV_NODISCARD
        const jsonv::path& path() const noexcept
        {
            return _path;
        }

        /// Human-readable details about the encountered problem.
        JSONV_NODISCARD
        const std::string& message() const noexcept
        {
            return _message;
        }

        /// If there was an exception that caused this problem, extra details can be found in the nested exception. This
        /// can be \c nullptr if there was no underlying cause.
        JSONV_NODISCARD
        const std::exception_ptr& nested_ptr() const noexcept
        {
            return _cause;
        }

    private:
        jsonv::path        _path;
        std::string        _message;
        std::exception_ptr _cause;
    };

    using problem_list = std::vector<problem>;

public:
    /// Create an \c extraction_error from the given list of \a problems.
    ///
    /// \param problems The list of problems which caused this error. It is expected that \c problems.size() is greater
    ///                 than \c 0. If it is not, a single \c problem will be created with a note about an unspecified
    ///                 error.
    explicit extraction_error(problem_list problems) noexcept;

    /// \{
    /// Create a new \c extraction_error with a single \c problem from the given \a path, \a message, and optional
    /// underlying \a cause.
    explicit extraction_error(jsonv::path path, std::string message, std::exception_ptr cause) noexcept;
    explicit extraction_error(jsonv::path path, std::string message) noexcept;
    explicit extraction_error(jsonv::path path, std::exception_ptr cause) noexcept;
    /// \}

    virtual ~extraction_error() noexcept;

    /// Get the path the first extraction error came from.
    JSONV_NODISCARD
    const jsonv::path& path() const noexcept;

    /// Get the first \c problem::cause. This can be \c nullptr if the first \c problem does not have an underlying
    /// cause.
    JSONV_NODISCARD
    const std::exception_ptr& nested_ptr() const noexcept;

    /// Get the list of problems which caused this \c extraction_error. There will always be at least one \c problem in
    /// this list.
    JSONV_NODISCARD
    const problem_list& problems() const noexcept { return _problems; }

private:
    template <typename... TArgs>
    explicit extraction_error(std::in_place_t, TArgs&&... problem_args) noexcept;

private:
    problem_list _problems;
};

/// Configuration for various extraction options. This becomes part of the \c extraction_context.
class JSONV_PUBLIC extract_options final
{
public:
    using size_type = extraction_error::problem_list::size_type;

    /// When an error is encountered during extraction, what should happen?
    enum class on_error
    {
        /// Immediately throw an \c extraction_error -- do not attempt to continue.
        fail_immediately,
        /// Attempt to continue extraction, collecting all errors and throwing at the end.
        collect_all,
    };

    /// When an object key has the same value as a previously-seen key, what should happen?
    enum class duplicate_key_action
    {
        /// Replace the previous value with the new one. The final value of the key in the object will be the
        /// last-encountered one.
        ///
        /// For example: `{ "a": 1, "a": 2, "a": 3 }` will end with `{ "a": 3 }`.
        replace,
        /// Ignore the new values. The final value of the key in the object will be the first-encountered one.
        ///
        /// For example: `{ "a": 1, "a": 2, "a": 3 }` will end with `{ "a": 1 }`.
        ignore,
        /// Repeated keys should raise an \c extraction_error.
        exception,
    };

public:
    /// Create an instance with the default options.
    extract_options() noexcept;

    ~extract_options() noexcept;

    /// Create a default set of options.
    JSONV_NODISCARD
    static extract_options create_default();

    /// \{
    /// See \c on_error. The default failure mode is \c fail_immediately.
    JSONV_NODISCARD
    on_error         failure_mode() const noexcept { return _failure_mode; };
    extract_options& failure_mode(on_error mode);
    /// \}

    /// \{
    /// The maximum allowed extractor failures the parser can encounter before throwing an error. This is only
    /// applicable if the \c failure_mode is not \c on_error::fail_immediately. By default, this value is 10.
    ///
    /// You should probably not set this value to an unreasonably high number, as each error encountered must be stored
    /// in memory for some period of time.
    JSONV_NODISCARD
    size_type        max_failures() const { return _max_failures; }
    extract_options& max_failures(size_type limit);
    /// \}

    /// \{
    /// See \c duplicate_key_action. The default action is \c replace.
    JSONV_NODISCARD
    duplicate_key_action on_duplicate_key() const { return _on_duplicate_key; }
    extract_options&     on_duplicate_key(duplicate_key_action action);
    /// \}

private:
    // For the purposes of ABI compliance, most modifications to the variables in this class should bump the minor
    // version number.
    on_error             _failure_mode     = on_error::fail_immediately;
    size_type            _max_failures     = 10U;
    duplicate_key_action _on_duplicate_key = duplicate_key_action::replace;
};

/// An \c extractor holds the method for converting JSON source into an arbitrary C++ type.
class JSONV_PUBLIC extractor
{
public:
    virtual ~extractor() noexcept;

    /// Get the run-time type this \c extractor knows how to extract. Once this \c extractor is registered with a
    /// \c formats, it is not allowed to change.
    JSONV_NODISCARD
    virtual const std::type_info& get_type() const = 0;

    /// Extract a the type \a from a \c value \a into a region of memory.
    ///
    /// \param context Extra information to help you decode sub-objects, such as looking up other \c formats. It also
    ///                tracks your \c path in the decoding heirarchy, so any exceptions thrown will have \c path
    ///                information in the error message.
    /// \param from The JSON \c value to extract something from.
    /// \param into The region of memory to create the extracted object in. There will always be enough room to create
    ///             your object and the alignment of the pointer should be correct (assuming a working \c alignof
    ///             implementation).
    virtual void extract(const extraction_context& context,
                         const value&              from,
                         void*                     into
                        ) const = 0;
};

class JSONV_PUBLIC extraction_context :
        public context
{
public:
    /// Create a new instance using the default \c formats (\c formats::global).
    extraction_context();

    /// Create a new instance using the given \a fmt, \a ver, \a p and \a userdata.
    explicit extraction_context(jsonv::formats                fmt,
                                std::optional<jsonv::version> ver      = std::nullopt,
                                jsonv::path                   p        = jsonv::path(),
                                const void*                   userdata = nullptr
                               );

    virtual ~extraction_context() noexcept;

    /// Get the current \c path this \c extraction_context is extracting for. This is useful when debugging and
    /// generating error messages.
    JSONV_NODISCARD
    const jsonv::path& path() const
    {
        return _path;
    }

    /// Attempt to extract a \c T from \a from using the \c formats associated with this context.
    ///
    /// \tparam T is the type to extract from \a from. It must be movable.
    ///
    /// \throws extraction_error if anything goes wrong when attempting to extract a value.
    template <typename T>
    JSONV_NODISCARD
    T extract(const value& from) const
    {
        alignas(T) unsigned char place[sizeof(T)];
        extract(typeid(T), from, static_cast<void*>(place));
        T* ptr = std::launder(reinterpret_cast<T*>(place));
        auto destroy = detail::on_scope_exit([ptr] { std::destroy_at(ptr); });
        return std::move(*ptr);
    }

    void extract(const std::type_info&     type,
                 const value&              from,
                 void*                     into
                ) const;

    /// Attempt to extract a \c T from <tt>from.at_path(subpath)</tt> using the \c formats associated with this context.
    ///
    /// \tparam T is the type to extract from \a from. It must be movable.
    ///
    /// \throws extraction_error if anything goes wrong when attempting to extract a value.
    template <typename T>
    JSONV_NODISCARD
    T extract_sub(const value& from, jsonv::path subpath) const
    {
        alignas(T) unsigned char place[sizeof(T)];
        extract_sub(typeid(T), from, std::move(subpath), static_cast<void*>(place));
        T* ptr = std::launder(reinterpret_cast<T*>(place));
        auto destroy = detail::on_scope_exit([ptr] { std::destroy_at(ptr); });
        return std::move(*ptr);
    }

    void extract_sub(const std::type_info& type, const value& from, jsonv::path subpath, void* into) const;

    /// Attempt to extract a \c T from <tt>from.at_path({elem})</tt> using the \c formats associated with this context.
    ///
    /// \tparam T is the type to extract from \a from. It must be movable.
    ///
    /// \throws extraction_error if anything goes wrong when attempting to extract a value.
    template <typename T>
    JSONV_NODISCARD
    T extract_sub(const value& from, path_element elem) const
    {
        return extract_sub<T>(from, jsonv::path({ elem }));
    }

private:
    jsonv::path _path;
};

/// Extract a C++ value from \a from using the provided \a fmts.
template <typename T>
JSONV_NODISCARD
T extract(const value& from, const formats& fmts)
{
    extraction_context context(fmts);
    return context.extract<T>(from);
}

/// Extract a C++ value from \a from using \c jsonv::formats::global().
template <typename T>
JSONV_NODISCARD
T extract(const value& from)
{
    extraction_context context;
    return context.extract<T>(from);
}

/// \}

}
