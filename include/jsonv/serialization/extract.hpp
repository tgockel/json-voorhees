/// \file jsonv/serialization/extract.hpp
/// Extract values from a JSON AST.
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
#include <jsonv/detail/scope_exit.hpp>
#include <jsonv/serialization/context.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>

#include <cstddef>
#include <exception>
#include <expected>
#include <string_view>
#include <type_traits>
#include <typeinfo>

namespace jsonv
{

class reader;

namespace detail
{

template <typename T> struct is_expected               : std::false_type { };
template <typename T, typename E> struct is_expected<std::expected<T, E>> : std::true_type { };

template <typename T> inline constexpr bool is_expected_v = is_expected<T>::value;

template <typename T> struct expected_value_or_self                            { using type = T; };
template <typename T, typename E> struct expected_value_or_self<std::expected<T, E>> { using type = T; };

template <typename T> using expected_value_or_self_t = typename expected_value_or_self<T>::type;

}

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
        const jsonv::path& path() const noexcept
        {
            return _path;
        }

        /// Human-readable details about the encountered problem.
        std::string_view message() const noexcept
        {
            return _message;
        }

        /// If there was an exception that caused this problem, extra details can be found in the nested exception. This
        /// can be \c nullptr if there was no underlying cause.
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
    const jsonv::path& path() const noexcept;

    /// Get the first \c problem::cause. This can be \c nullptr if the first \c problem does not have an underlying
    /// cause.
    const std::exception_ptr& nested_ptr() const noexcept;

    /// Get the list of problems which caused this \c extraction_error. There will always be at least one \c problem in
    /// this list.
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
    static extract_options create_default();

    /// \{
    /// See \c on_error. The default failure mode is \c fail_immediately.
    on_error         failure_mode() const noexcept { return _failure_mode; };
    extract_options& failure_mode(on_error mode);
    /// \}

    /// \{
    /// The maximum allowed extractor failures the parser can encounter before throwing an error. This is only
    /// applicable if the \c failure_mode is not \c on_error::fail_immediately. By default, this value is 10.
    ///
    /// You should probably not set this value to an unreasonably high number, as each error encountered must be stored
    /// in memory for some period of time.
    size_type        max_failures() const { return _max_failures; }
    extract_options& max_failures(size_type limit);
    /// \}

    /// \{
    /// See \c duplicate_key_action. The default action is \c replace.
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

class JSONV_PUBLIC extraction_context :
        public context
{
public:
    using problem_list = extraction_error::problem_list;

public:
    /// Create a new instance using the default \c formats and \c extract_options.
    explicit extraction_context();

    /// \{
    /// Create a new instance using the given \a formats, \a options, optional \a version, and optional \a user_data.
    explicit extraction_context(jsonv::formats                  formats,
                                const extract_options&          options,
                                const std::optional<jsonv::version>& version   = std::nullopt,
                                const void*                     user_data = nullptr
                               );
    explicit extraction_context(jsonv::formats                  formats,
                                const std::optional<jsonv::version>& version   = std::nullopt,
                                const void*                     user_data = nullptr
                               );
    /// \}

    virtual ~extraction_context() noexcept override;

    /// Note that a problem has been encountered. The behavior of this function depends on the \c extract_options this
    /// context was created with. If \c extract_options::failure_mode is \c extract_options::on_error::fail_immediately
    /// (the default), this will throw an \c extraction_error with the created \c extraction_error::problem. If the mode
    /// is \c extract_options::on_error::collect_all, created problems will be added to a list until the
    /// \c extract_options::max_failure count is reached, at which point an \c extraction_error will be thrown will all
    /// encountered problems.
    ///
    /// \returns \c error in all cases. This is done so \c extractor implementations can easily write
    ///          `return context.problem(...);`.
    ///
    /// \code
    /// // NOTE: This is directly using the extractor interface, which requires us to work with the void* for place.
    /// // It is preferable to use extractor_for or adapter_for.
    /// std::expected<void, jsonv::ast_node_type>
    /// bounded_extractor::extract(jsonv::extraction_context& context,
    ///                            jsonv::reader&             reader,
    ///                            void*                      place
    ///                           ) const
    /// {
    ///     if (auto result = context.extract<std::int32_t>(reader))
    ///     {
    ///         if (*result < 500 || *result > 2500)
    ///         {
    ///             return context.problem(reader.current_path(), "Expected a value between 500 and 2500");
    ///         }
    ///         else
    ///         {
    ///             std::memcpy(place, &*result, sizeof *result);
    ///             return {};
    ///         }
    ///     }
    ///     else
    ///     {
    ///         return std::unexpected(result.error());
    ///     }
    /// }
    /// \endcode
    ///
    /// The returned \c std::unexpected is implicitly convertible to any \c std::expected<T, ast_node_type>, so
    /// extractor implementations can simply \c return the value of \c problem from any function in the extract
    /// pipeline.
    template <typename... TArgs>
    [[nodiscard]] std::unexpected<ast_node_type> problem(TArgs&&... args)
    {
        // If we're the first error, reserve the max amount.
        if (_problems.empty() && _options.failure_mode() == extract_options::on_error::collect_all)
            _problems.reserve(_options.max_failures());

        _problems.emplace_back(std::forward<TArgs>(args)...);
        on_problem();
        return std::unexpected(ast_node_type::error);
    }

    /// \{
    /// Get the extraction problems that have been encountered so far. If this list is empty, no problems have occurred.
    const problem_list& problems() const& { return _problems; }
    problem_list&&      problems() &&     { return std::move(_problems); }
    /// \}

    /// \{
    /// Check that the \c reader::current AST node of \a from has the given \a type. If it does not, an \c error is
    /// returned and a \ref problem will be added with a description of the mismatched types.
    ///
    /// \see current_as
    /// \see reader::expect
    std::expected<void, ast_node_type> expect(reader& from, ast_node_type type);
    std::expected<void, ast_node_type> expect(reader& from, std::initializer_list<ast_node_type> types);
    /// \}

    /// Get the \c reader::current AST node of \a from if it has the type of \c TAstNode. If the current type does not
    /// match the expected type, an \c error is returned and a \ref problem will be added with a description of the
    /// mismatched types.
    ///
    /// \see expect
    /// \see reader::current_as
    template <typename TAstNode>
    std::expected<TAstNode, ast_node_type> current_as(reader& from)
    {
        return expect(from, TAstNode::type()).transform([&] { return from.current().as<TAstNode>(); });
    }

    /// Attempt to extract a \c T from \a from using the \c formats associated with this context.
    ///
    /// \tparam T is the type to extract from \a from. It must be movable.
    template <typename T>
    std::expected<T, ast_node_type> extract(reader& from)
    {
        alignas(T) std::byte place[sizeof(T)];
        T* ptr = reinterpret_cast<T*>(&place[0]);
        if (auto res = extract(typeid(T), from, static_cast<void*>(ptr)))
        {
            auto destroy = detail::on_scope_exit([ptr] { ptr->~T(); });
            return std::move(*ptr);
        }
        else
        {
            return std::unexpected(res.error());
        }
    }

    [[nodiscard]]
    std::expected<void, ast_node_type> extract(const std::type_info& type, reader& from, void* into);

    /// The path currently being extracted -- updated by \c path_scope guards pushed and popped by adapters as they
    /// descend into nested members. Used for error messages.
    const jsonv::path& path() const noexcept { return _path; }

    /// RAII guard which appends a path element while live and restores the previous path on destruction. Adapters use
    /// this to give nested members a meaningful path in error messages even when the underlying reader has been
    /// re-rooted (e.g. by walking a sub-tree through \c read_value).
    class JSONV_PUBLIC path_scope
    {
    public:
        path_scope(extraction_context& ctx, path_element elem) :
                _ctx(&ctx),
                _saved(ctx._path)
        {
            ctx._path += std::move(elem);
        }

        path_scope(const path_scope&)            = delete;
        path_scope& operator=(const path_scope&) = delete;

        ~path_scope() noexcept
        {
            if (_ctx)
                _ctx->_path = std::move(_saved);
        }

    private:
        extraction_context* _ctx;
        jsonv::path         _saved;
    };

private:
    void on_problem();

private:
    extract_options _options;
    problem_list    _problems;
    jsonv::path     _path;
};

/// An \c extractor holds the method for converting JSON source into an arbitrary C++ type.
class JSONV_PUBLIC extractor
{
public:
    virtual ~extractor() noexcept;

    /// Get the run-time type this \c extractor knows how to extract. Once this \c extractor is registered with a
    /// \c formats, it is not allowed to change.
    virtual const std::type_info& get_type() const noexcept = 0;

    /// Extract a the type \a from a reader \a into a region of memory.
    ///
    /// \param context Extra information to help you decode sub-objects, such as looking up other \c extractor
    ///                implementations via \c formats.
    /// \param from The JSON \c reader to extract something from.
    /// \param into The region of memory to create the extracted object in. There will always be enough room to create
    ///             your object and the alignment of the pointer should be correct (assuming a working \c alignof
    ///             implementation).
    ///
    /// \returns A success result on successful extraction; an \c std::unexpected if extraction failed for some reason.
    ///          The error value carries the AST node type involved when the failure was a type-mismatch and
    ///          \c ast_node_type::error as a sentinel otherwise. The \a context will have additional information about
    ///          the failure in \c extraction_context::problems.
    [[nodiscard]]
    virtual std::expected<void, ast_node_type>
    extract(extraction_context& context, reader& from, void* into) const = 0;
};

/// Consume the JSON subtree under \a from starting at \c reader::current and return it as a fully materialised \c value
/// tree. On return, \a from will have advanced one position past the consumed subtree, just as \c reader::next_structure
/// would have done after the same input.
///
/// This is provided as a bridge so older \c value-based adapters (\c optional_adapter, \c wrapper_adapter,
/// \c polymorphic_adapter and the \c serialization_builder DSL) can keep operating on \c value trees while the
/// surrounding pipeline runs against a streaming \c reader.
JSONV_PUBLIC value read_value(reader& from);

/// \{
template <typename T>
T extract(reader& from, const formats& fmts)
{
    extraction_context cxt(fmts);
    if (from.good() && from.current().type() == ast_node_type::document_start)
    {
        const bool advanced JSONV_UNUSED = from.next_token();
    }

    if (auto res = cxt.extract<T>(from))
        return std::move(res).value();
    else
        throw extraction_error(std::move(cxt).problems());
}

template <typename T>
T extract(reader&& from, const formats& fmts)
{
    return extract<T>(from, fmts);
}

template <typename T>
T extract(reader& from)
{
    return extract<T>(from, formats::global());
}

template <typename T>
T extract(reader&& from)
{
    return extract<T>(from);
}

/// \}

/// TODO(#150): Something
/// Extract a C++ value from \a from using the provided \a fmts.
template <typename T>
T extract(const value& from, const formats& fmts)
{
    reader rdr(from);
    return extract<T>(rdr, fmts);
}

/// TODO(#150): Something
/// Extract a C++ value from \a from using \c jsonv::formats::global().
template <typename T>
T extract(const value& from)
{
    return extract<T>(from, formats::global());
}

/// \}

}
