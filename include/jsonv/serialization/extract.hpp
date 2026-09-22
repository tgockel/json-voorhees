/// \file jsonv/serialization/extract.hpp
/// Extraction of C++ types from a JSON AST.
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
#include <jsonv/ast.hpp>
#include <jsonv/detail/scope_exit.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization/context.hpp>
#include <jsonv/value.hpp>

#include <concepts>
#include <cstddef>
#include <exception>
#include <expected>
#include <initializer_list>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

namespace jsonv
{

namespace detail
{

class borrowed_subtree;

/// \{
/// Check if \c T is a \c std::expected and, if it is, get the type it holds.
///
/// Extraction functions are allowed to return either a bare \c T or a \c std::expected<T, ast_node_type>, so the
/// machinery which deduces what an adapter extracts has to see through the latter.
template <typename T>
struct is_expected :
        std::false_type
{ };

template <typename T, typename E>
struct is_expected<std::expected<T, E>> :
        std::true_type
{ };

template <typename T>
inline constexpr bool is_expected_v = is_expected<T>::value;

template <typename T>
struct expected_value_or_self
{
    using type = T;
};

template <typename T, typename E>
struct expected_value_or_self<std::expected<T, E>>
{
    using type = T;
};

template <typename T>
using expected_value_or_self_t = typename expected_value_or_self<T>::type;
/// \}

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
    virtual const std::type_info& get_type() const noexcept = 0;

    /// Extract the type \a from a \c reader \a into a region of memory.
    ///
    /// \param context Extra information to help you decode sub-objects, such as looking up other \c extractor
    ///                implementations via \c formats. It is also where a \ref extraction_context::problem is recorded
    ///                and where the \c path a problem is reported at comes from.
    /// \param from The JSON \c reader to extract something from. On entry, \c reader::current is the first node of the
    ///             value to extract; on a successful return it should be one position past that value, as
    ///             \c reader::next_value would leave it.
    /// \param into The region of memory to create the extracted object in. There will always be enough room to create
    ///             your object and the alignment of the pointer should be correct (assuming a working \c alignof
    ///             implementation).
    ///
    /// \returns A success result if the object was created in \a into; otherwise a \c std::unexpected carrying the
    ///          \c ast_node_type actually found when the failure was a type mismatch, or \c ast_node_type::error as a
    ///          sentinel for everything else. In the failure case nothing has been constructed in \a into and
    ///          \c extraction_context::problems describes what went wrong.
    ///
    /// \see extractor_for
    /// \see adapter_for
    /// \see value_adapter_for
    JSONV_NODISCARD
    virtual std::expected<void, ast_node_type>
    extract(extraction_context& context, reader& from, void* into) const = 0;
};

/// Provides extra information to routines used for extraction, collects the problems they encounter, and tracks where
/// in the document they are.
///
/// Unlike a \c serialization_context, this is mutable and single-use: recording a problem changes it. It is neither
/// copyable nor movable, since a \ref path_scope holds a pointer to the instance it was pushed onto.
class JSONV_PUBLIC extraction_context :
        public context
{
public:
    using problem_list = extraction_error::problem_list;

    class path_scope;

public:
    /// Create a new instance using the default \c formats (\c formats::global).
    extraction_context();

    /// Create a new instance using the given \a fmt, \a ver, \a p and \a userdata.
    ///
    /// \param p A path all reported problems are relative to. This is almost always empty -- it exists for extraction
    ///          of a document which is itself a fragment of some larger one.
    explicit extraction_context(jsonv::formats                fmt,
                                std::optional<jsonv::version> ver      = std::nullopt,
                                jsonv::path                   p        = jsonv::path(),
                                const void*                   userdata = nullptr
                               );

    extraction_context(const extraction_context&)            = delete;
    extraction_context& operator=(const extraction_context&) = delete;

    virtual ~extraction_context() noexcept;

    /// Is the \c value being extracted storage the pipeline materialised for the occasion, rather than storage the
    /// caller handed in?
    ///
    /// An extractor which returns a view of what it was given must check this and refuse when it is \c true, because
    /// the storage its view would name is destroyed as the bridge unwinds. \c std::string_view is the built-in one;
    /// the situation arises whenever a \c value -based adapter runs against a reader over JSON text, since there is
    /// no pre-existing tree for it to borrow and one has to be built.
    ///
    /// This is true for everything nested under such a materialisation, not only the value that caused it: a
    /// \c std::vector<std::string_view> extracted from text is materialised once at the container and each element
    /// then borrows from that temporary.
    JSONV_NODISCARD
    bool source_is_temporary() const noexcept { return _materialised_depth != 0U; }

    /// Get the path currently being extracted, as named by the live \ref path_scope guards.
    ///
    /// This is built on demand by walking the scope chain, so it is not free -- but nothing on a successful extraction
    /// calls it. If no scope is live the result is the base path this context was created with, which is usually empty;
    /// see \ref path_scope for why that is not the same as "the root of the document".
    JSONV_NODISCARD
    jsonv::path path() const;

    /// Note that a problem has been encountered, forwarding \a args to an \c extraction_error::problem.
    ///
    /// \returns \c std::unexpected of \c ast_node_type::error in all cases, which converts implicitly into any
    ///          \c std::expected<T, ast_node_type>, so an implementation can simply return it:
    ///
    /// \code
    /// if (*result < 500 || *result > 2500)
    ///     return context.problem(context.path(), "Expected a value between 500 and 2500");
    /// \endcode
    ///
    /// Recording a problem does not throw. The entry point which started extraction throws a single
    /// \c extraction_error carrying everything collected, once the pipeline has unwound.
    template <typename... TArgs>
    JSONV_NODISCARD
    std::unexpected<ast_node_type> problem(TArgs&&... args)
    {
        _problems.emplace_back(std::forward<TArgs>(args)...);
        return std::unexpected(ast_node_type::error);
    }

    /// \{
    /// Get the problems encountered so far. If this list is empty, no problems have occurred.
    JSONV_NODISCARD
    const problem_list& problems() const& { return _problems; }
    JSONV_NODISCARD
    problem_list&&      problems() &&     { return std::move(_problems); }
    /// \}

    /// \{
    /// Check that the \c reader::current AST node of \a from has the given \a type or is one of the given \a types. If
    /// it is not, a \ref problem describing the mismatch is recorded and the type actually found is returned.
    ///
    /// This is \c reader::expect plus the human-readable message, which lives here because this is the layer that has
    /// the path and the problem list to attach it to.
    ///
    /// \see current_as
    /// \see reader::expect
    JSONV_NODISCARD
    std::expected<void, ast_node_type> expect(reader& from, ast_node_type type);
    JSONV_NODISCARD
    std::expected<void, ast_node_type> expect(reader& from, std::initializer_list<ast_node_type> types);
    /// \}

    /// Get the \c reader::current AST node of \a from as a \c TAstNode, recording a \ref problem if it is some other
    /// type.
    ///
    /// \see expect
    /// \see reader::current_as
    template <typename TAstNode>
    JSONV_NODISCARD
    std::expected<TAstNode, ast_node_type> current_as(reader& from)
    {
        // Written as an explicit branch rather than `expect(...).transform(...)` for the same reason
        // `reader::current_as` is: the monadic operations on `std::expected` postdate the type, so using one here
        // would quietly raise the minimum toolchain by a release.
        if (auto matched = expect(from, TAstNode::type()); !matched)
            return std::unexpected(matched.error());
        else
            return from.current().as<TAstNode>();
    }

    /// \{
    /// Attempt to extract a \c T from \a from using the \c formats associated with this context.
    ///
    /// \tparam T is the type to extract. It must be movable.
    template <typename T>
    JSONV_NODISCARD
    std::expected<T, ast_node_type> extract(reader& from)
    {
        alignas(T) std::byte place[sizeof(T)];
        if (auto result = extract(typeid(T), from, static_cast<void*>(place)); !result)
            return std::unexpected(result.error());

        T*   ptr     = std::launder(reinterpret_cast<T*>(place));
        auto destroy = detail::on_scope_exit([ptr] { std::destroy_at(ptr); });
        return std::move(*ptr);
    }

    JSONV_NODISCARD
    std::expected<void, ast_node_type> extract(const std::type_info& type, reader& from, void* into);
    /// \}

    /// \{
    /// Attempt to extract a \c T from the in-memory \a from using the \c formats associated with this context.
    ///
    /// These run the same pipeline as the \c reader overloads by walking \a from through a \c reader::from_value, and
    /// report failure by throwing rather than by returning. They are how an adapter written against the older
    /// \c value-based interface reaches the rest of the pipeline.
    ///
    /// \throws extraction_error if anything goes wrong when attempting to extract a value.
    ///
    /// \see value_adapter_for
    template <typename T>
    JSONV_NODISCARD
    T extract(const value& from)
    {
        alignas(T) std::byte place[sizeof(T)];
        extract(typeid(T), from, static_cast<void*>(place));
        T*   ptr     = std::launder(reinterpret_cast<T*>(place));
        auto destroy = detail::on_scope_exit([ptr] { std::destroy_at(ptr); });
        return std::move(*ptr);
    }

    void extract(const std::type_info& type, const value& from, void* into);
    /// \}

    /// \{
    /// Attempt to extract a \c T from <tt>from.at_path(subpath)</tt> using the \c formats associated with this context,
    /// reporting any problem under \a subpath.
    ///
    /// \throws extraction_error if anything goes wrong when attempting to extract a value.
    template <typename T>
    JSONV_NODISCARD
    T extract_sub(const value& from, jsonv::path subpath)
    {
        alignas(T) std::byte place[sizeof(T)];
        extract_sub(typeid(T), from, std::move(subpath), static_cast<void*>(place));
        T*   ptr     = std::launder(reinterpret_cast<T*>(place));
        auto destroy = detail::on_scope_exit([ptr] { std::destroy_at(ptr); });
        return std::move(*ptr);
    }

    void extract_sub(const std::type_info& type, const value& from, jsonv::path subpath, void* into);

    template <typename T>
    JSONV_NODISCARD
    T extract_sub(const value& from, path_element elem)
    {
        return extract_sub<T>(from, jsonv::path({ std::move(elem) }));
    }
    /// \}

    /// An RAII guard naming one step of the extraction path while it is alive.
    ///
    /// A \c reader knows where the *reader* is, which is not always where the *extractor* is: an adapter which
    /// re-roots onto a subtree gets a reader whose \c reader::current_path is relative to that subtree, and an adapter
    /// which renames a member wants the name the caller declared rather than the one the document used. Pushing a
    /// scope says where the extractor is, and takes precedence over the reader's own answer.
    ///
    /// Scopes are kept on the C++ stack and linked into a chain, so a push is two stores and a pop is one. No
    /// \c jsonv::path is built until something calls \c extraction_context::path, which happens only when a problem is
    /// recorded.
    ///
    /// The \c std::size_t and \c std::string_view overloads allocate nothing; the \a key of the latter must outlive
    /// the scope, which is why an owning \c path_element overload exists for the callers that cannot promise it (a key
    /// decoded from an \c ast_node_type::key_escaped node, for instance).
    class JSONV_PUBLIC path_scope
    {
    public:
        path_scope(extraction_context& context, std::size_t index) noexcept;
        path_scope(extraction_context& context, std::string_view key) noexcept;
        path_scope(extraction_context& context, path_element elem);

        /// Push every element of \a subpath at once. \a subpath must outlive the scope.
        path_scope(extraction_context& context, const jsonv::path& subpath) noexcept;

        path_scope(const path_scope&)            = delete;
        path_scope& operator=(const path_scope&) = delete;

        ~path_scope() noexcept;

    private:
        friend class extraction_context;

        /// Append this scope's ancestors and then itself to \a out, so the result reads outermost-first.
        void append_to(jsonv::path& out) const;

    private:
        extraction_context*                                                     _context;
        const path_scope*                                                       _parent;
        std::variant<std::size_t, std::string_view, path_element, const jsonv::path*> _element;
    };

private:
    friend class path_scope;
    friend class detail::borrowed_subtree;

    /// The path to report a problem encountered at \c reader::current of \a from. This is \ref path when any scope has
    /// named a position and the reader's own \c reader::current_path when none has -- never both, since an adapter
    /// walking a single reader would otherwise have its position counted twice.
    /// Where to report a problem noticed while the reader is sitting on the thing that is wrong -- what \c expect and
    /// \c current_as want. The reader's own position is right, so nothing else is consulted.
    JSONV_NODISCARD
    jsonv::path problem_path(const reader& from) const;

    /// Where to report a failure which is being translated out of an exception. Unlike \ref problem_path this takes
    /// the location a bridge left behind on its way out, because by now the cursor has moved on from the value which
    /// failed. Taking it is the point: it belongs to the failure being translated and to nothing after it.
    JSONV_NODISCARD
    jsonv::path take_failure_path(const reader& from);

    /// Remove and return the problems recorded since \a mark. Used by the \c value -based overloads to move what they
    /// collected into the \c extraction_error they throw, so the catch which folds that error back onto a context does
    /// not record it twice.
    JSONV_NODISCARD
    problem_list take_problems_since(problem_list::size_type mark);

private:
    jsonv::path       _base_path;
    const path_scope* _innermost          = nullptr;
    std::size_t       _materialised_depth = 0U;

    /// Where to report the failure currently unwinding, left by a bridge which walked the cursor past the value which
    /// failed. A destructor runs before the handler which records the problem, so the location has to be worked out
    /// in the destructor and picked up by \ref take_failure_path. It lives and dies with one call to \c extract.
    std::optional<jsonv::path> _failure_path;

    problem_list      _problems;
};

/// Consume the JSON subtree under \a from starting at \c reader::current and return it as a fully materialised \c value
/// tree.
///
/// On return \a from has advanced one position past the consumed subtree, exactly as \c reader::next_value would have
/// left it for the same input. Every adapter reading a subtree has to agree on this, or subtrees get consumed twice or
/// not at all. A leading \c ast_node_type::document_start is stepped over first, so this works on a freshly-created
/// reader as well as on one positioned mid-document.
///
/// This is the bridge which lets adapters written against the older \c value -based interface keep working while the
/// surrounding pipeline runs against a streaming \c reader.
///
/// \throws extraction_error if \a from is not positioned on a value or the document ends part-way through one.
///
/// \see value_adapter_for
JSONV_NODISCARD JSONV_PUBLIC value read_value(reader& from);

namespace detail
{

/// The subtree under a reader's cursor as a \c value, borrowed rather than copied when the reader can lend it.
///
/// A reader created by \c reader::from_value is already holding the tree the older \c value -based interface wants.
/// Materialising a copy for it would pay for a deep copy and, worse, hand the adapter storage which dies with this
/// object -- silently breaking every extractor which returns a view of what it was given. Borrowing is what keeps a
/// \c std::string_view pointing into the caller's \c value, which is where it pointed before extraction ran through
/// a \c reader.
///
/// A reader over JSON text has no such tree, so the subtree is materialised. Anything borrowed from it is valid only
/// until this object goes away, which is why the context is told: see \c extraction_context::source_is_temporary.
///
/// **The reader is not advanced until \c commit.** An adapter on the bridge consumes its whole subtree before the
/// older body runs, so a failure in that body would otherwise be reported against the next sibling. Leaving the cursor
/// where the extraction started means the position is simply still correct, which is cheaper and more accurate than
/// noting it beforehand -- \c reader::current_path rebuilds by scanning from the start of the document on a
/// text-backed source, so asking for it on every successful extraction is quadratic. A structure read from text is the
/// one case which cannot wait, since materialising it is what walks the cursor over it.
class JSONV_PUBLIC borrowed_subtree
{
public:
    borrowed_subtree(extraction_context& context, reader& from);

    borrowed_subtree(const borrowed_subtree&)            = delete;
    borrowed_subtree& operator=(const borrowed_subtree&) = delete;

    ~borrowed_subtree() noexcept;

    JSONV_NODISCARD
    const value& get() const noexcept { return _borrowed ? *_borrowed : _owned; }

    /// Step the reader past the subtree, if it is not already past it. Call this once the older body has succeeded;
    /// skipping it on failure is what leaves the cursor naming the value which failed.
    void commit();

private:
    extraction_context* _context;
    reader*             _from;
    const value*        _borrowed;
    bool                _materialised;
    bool                _advanced;
    int                 _uncaught_on_entry;
    value               _owned;
};

/// Call \a func as an extraction function and normalise whatever it gives back into a \c std::expected.
///
/// Four call shapes are accepted, tried in this order: <tt>(context, reader)</tt>, <tt>(reader)</tt>,
/// <tt>(context, value)</tt>, <tt>(value)</tt>. The last two are the interface functions were written against before
/// extraction ran off a \c reader; they get a subtree materialised by \c read_value. Either a bare \c T or a
/// \c std::expected<T, ast_node_type> is an acceptable return.
template <typename T, typename FExtract>
JSONV_NODISCARD
std::expected<T, ast_node_type> invoke_extract(const FExtract& func, extraction_context& context, reader& from)
{
    auto normalise = [](auto&& result) -> std::expected<T, ast_node_type>
                     {
                         if constexpr (is_expected_v<std::remove_cvref_t<decltype(result)>>)
                         {
                             if (result)
                                 return std::move(result).value();
                             else
                                 return std::unexpected(result.error());
                         }
                         else
                         {
                             return std::forward<decltype(result)>(result);
                         }
                     };

    if constexpr (std::invocable<const FExtract&, extraction_context&, reader&>)
    {
        return normalise(func(context, from));
    }
    else if constexpr (std::invocable<const FExtract&, reader&>)
    {
        return normalise(func(from));
    }
    else if constexpr (std::invocable<const FExtract&, extraction_context&, const value&>)
    {
        // `get()` is a `const value&`, which is the signature the `invocable` check above tested. Handing over a
        // mutable one would let a callable overloaded on both pick the other overload.
        borrowed_subtree subtree(context, from);
        auto             result = normalise(func(context, subtree.get()));
        if (result)
            subtree.commit();
        return result;
    }
    else
    {
        static_assert(std::invocable<const FExtract&, const value&>,
                      "An extraction function must be callable as (extraction_context&, reader&), (reader&), "
                      "(extraction_context&, const value&) or (const value&)"
                     );

        borrowed_subtree subtree(context, from);
        auto             result = normalise(func(subtree.get()));
        if (result)
            subtree.commit();
        return result;
    }
}

/// The type an extraction function extracts: its return type, with a \c std::expected unwrapped, deduced from the
/// same four call shapes \c invoke_extract accepts and in the same order.
template <typename FExtract>
struct extract_function_result
{
    static auto deduce()
    {
        if constexpr (std::invocable<const FExtract&, extraction_context&, reader&>)
            return std::type_identity<std::invoke_result_t<const FExtract&, extraction_context&, reader&>>();
        else if constexpr (std::invocable<const FExtract&, reader&>)
            return std::type_identity<std::invoke_result_t<const FExtract&, reader&>>();
        else if constexpr (std::invocable<const FExtract&, extraction_context&, const value&>)
            return std::type_identity<std::invoke_result_t<const FExtract&, extraction_context&, const value&>>();
        else
            return std::type_identity<std::invoke_result_t<const FExtract&, const value&>>();
    }

    using type = expected_value_or_self_t<std::remove_cvref_t<typename decltype(deduce())::type>>;
};

template <typename FExtract>
using extract_function_result_t = typename extract_function_result<FExtract>::type;

}

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
