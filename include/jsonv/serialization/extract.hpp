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
#include <jsonv/forward.hpp>
#include <jsonv/parse.hpp>
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

/// Does the source an extraction reads from outlive it?
enum class source_lifetime : unsigned char
{
    /// The caller owns the source and keeps it alive past the extraction, so what is extracted may view it.
    caller,
    /// The source was handed to the extraction to own and is freed when it finishes, so nothing extracted may view it.
    extraction,
};

/// The one place a public entry point runs an extraction: every \c jsonv::extract overload and
/// \c extraction_context::extract(const value&) come through here.
///
/// A reader on \c ast_node_type::document_start is extracted as a whole document. Its source is checked to have
/// parsed, the \c document_start is stepped over -- here and nowhere else, so an \c extractor is never entered on one
/// -- and once the value has been read the reader must be on \c ast_node_type::document_end, where it is left. A
/// reader the caller has already positioned gets none of that: the value under the cursor is extracted and the cursor
/// is left one past it, as \c reader::next_value would. A reader on an \c ast_node_type::error node, positioned or
/// not, has its source checked as well, since there is no value there and the parse can say why.
///
/// \param into Storage for the extracted object, as for \c extractor::extract.
/// \param destroy Destroys the object in \a into. A document with something after its value is only found to have it
///                once the object has been built, and has to be refused after all.
///
/// \throws extraction_error carrying the problems this call recorded, and only those, since a \c value bridge calls
///                          this with a context which may already hold some.
JSONV_PUBLIC void extract_entry(extraction_context&   context,
                                const std::type_info& type,
                                reader&               from,
                                void*                 into,
                                void               (* destroy)(void*) noexcept,
                                source_lifetime       lifetime
                               );

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
        /// Report the first problem and stop, so the \c extraction_error thrown describes one thing that went wrong.
        fail_immediately,
        /// Keep extracting past a problem wherever something knows how to resume, so the \c extraction_error thrown
        /// at the end describes as many of them as it can.
        ///
        /// Resuming is only possible where a composite knows where its next element begins -- the next element of an
        /// array, the next key of an object -- which is why \c extraction_context::recover is asked rather than told.
        /// A failure with no enclosing composite to resume into still ends extraction with a single problem.
        ///
        /// Collecting gathers diagnostics; it does not produce partially-extracted objects. An extraction which
        /// recovered from anything still throws, so this changes how much the error explains and never whether one
        /// happens.
        ///
        /// \see extract_options::max_failures
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
    /// The number of problems to collect before giving up. This is only applicable if the \c failure_mode is
    /// \c on_error::collect_all. By default, this value is 10.
    ///
    /// This is a threshold extraction stops at rather than a cap on the list it reports. A single failure which
    /// reports several problems at once -- an adapter recording a batch of them before returning, or throwing an
    /// \c extraction_error carrying several -- is taken whole rather than torn in half, so the final list can exceed
    /// the limit by that batch. Truncating would drop diagnostics to enforce a bound whose purpose is to stop the
    /// walk, not to edit the report.
    ///
    /// A limit of \c 0 or \c 1 makes the first problem the last, which is \c on_error::fail_immediately in all but
    /// name.
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
    ///             value to extract -- never \c ast_node_type::document_start, which the entry points step over
    ///             before any \c extractor runs. On a successful return it should be one position past that value, as
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

    /// Create a new instance using the given \a fmt, \a ver, \a p, \a userdata and \a options.
    ///
    /// \param p A path all reported problems are relative to. This is almost always empty -- it exists for extraction
    ///          of a document which is itself a fragment of some larger one.
    /// \param options What to do when something goes wrong. The default reports the first problem and stops; see
    ///                \c extract_options::on_error.
    explicit extraction_context(jsonv::formats                fmt,
                                std::optional<jsonv::version> ver      = std::nullopt,
                                jsonv::path                   p        = jsonv::path(),
                                const void*                   userdata = nullptr,
                                extract_options               options  = extract_options()
                               );

    extraction_context(const extraction_context&)            = delete;
    extraction_context& operator=(const extraction_context&) = delete;

    virtual ~extraction_context() noexcept;

    /// Is the source being extracted storage which is freed when extraction finishes, rather than storage the caller
    /// keeps?
    ///
    /// An extractor which returns a view of what it was given must check this and refuse when it is \c true, because
    /// the storage its view would name is gone by the time the caller has it. \c std::string_view is the built-in one.
    /// There are two ways to get here:
    ///
    ///  - A \c value -based adapter runs against a reader over JSON text. There is no pre-existing tree for it to
    ///    borrow, so one is materialised and destroyed as the bridge unwinds. This is true for everything nested
    ///    under such a materialisation, not only the value that caused it.
    ///  - The source was handed to extraction to own: \c jsonv::extract given a \c std::string rvalue, or an rvalue
    ///    \c reader which owns its source. That source dies with the call, so this is true for the whole extraction.
    JSONV_NODISCARD
    bool source_is_temporary() const noexcept { return _temporary_source_depth != 0U; }

    /// Get the options this context is extracting under.
    JSONV_NODISCARD
    const extract_options& options() const noexcept { return _options; }

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
    /// May extraction recover from a failure and keep going?
    ///
    /// A composite which knows where its next element begins -- the next element of an array, the next key of an
    /// object -- asks this when one of them fails. A \c true answer means skip what failed and keep walking, so one
    /// bad element does not hide every problem after it; \c false means report the failure and let the pipeline
    /// unwind. Only the loop knows where it would resume, which is why collecting is something a composite opts into
    /// rather than something this context can deliver on its own -- and why a failure with no enclosing composite
    /// ends extraction however \c extract_options::failure_mode is set.
    ///
    /// The answer is \c false under \c extract_options::on_error::fail_immediately, and becomes \c false in
    /// \c collect_all once \c extract_options::max_failures problems have been recorded. It is never a promise that
    /// extraction will succeed: recovering collects diagnostics, it does not produce partial objects, so a composite
    /// which recovered from anything **must still report failure** once its loop is done.
    ///
    /// \code
    /// auto element = context.extract<T>(from);
    /// if (!element)
    /// {
    ///     if (!context.recover())
    ///         return std::unexpected(element.error());
    ///
    ///     recovered = true;
    ///     context.skip_failed_value(from);
    ///     continue;
    /// }
    /// \endcode
    ///
    /// Note \ref skip_failed_value rather than \c reader::next_value: where the value which failed was read through
    /// the \c value bridge, the cursor is already past it and stepping again would skip the next one.
    ///
    /// The overload taking an \c extraction_error is for an adapter on the \c value bridge, which reports failure by
    /// throwing. On \c true the problems \a ex carries have been folded onto this context and the caller may
    /// continue; on \c false nothing was folded and the caller should rethrow \a ex, which the catch in
    /// \c extract(const std::type_info&, reader&, void*) folds instead. Either way every problem is recorded exactly
    /// once, which is the thing to preserve: the \c value -based overloads hand their problems to the exception
    /// rather than leaving them behind, so a fold in both places would report each failure twice.
    JSONV_NODISCARD
    bool recover() const noexcept;
    JSONV_NODISCARD
    bool recover(const extraction_error& ex);
    /// \}

    /// Remove and return the problems recorded since \a mark, a value \c problems() previously reported the size of.
    ///
    /// A composite which recovered still has to report failure, and on the \c value -based interface that means
    /// throwing an \c extraction_error. This is how it hands over what it collected without leaving a copy behind for
    /// the catch which folds that error back onto a context to record a second time.
    JSONV_NODISCARD
    problem_list take_problems_since(problem_list::size_type mark);

    /// Step \a from past the value whose failure is being recovered from.
    ///
    /// This is \c reader::next_value, except where the step has already happened. Most extractors leave the cursor
    /// naming the value they rejected, so stepping over it is exactly one \c reader::next_value. An adapter on the
    /// \c value bridge reading a structure out of JSON text is the exception: materialising that structure is what
    /// walks the cursor over it, so by the time the older body reports a failure the cursor names the *next* sibling.
    /// A loop recovering with a bare \c reader::next_value steps over that sibling as well, dropping it from the
    /// result and dropping every problem it had to report -- and misnumbering everything after it.
    ///
    /// The note this consults belongs to \a from and to the one failure being reported. It is cleared when the next
    /// extraction starts, so a note nobody collects expires rather than answering for an unrelated position.
    ///
    /// \see recover
    /// \see note_value_consumed
    void skip_failed_value(reader& from) noexcept;

    /// Note that the failure about to be reported has already consumed, from \a from, the value it failed on, so
    /// whatever recovers from it must not step over that value a second time.
    ///
    /// The \c value bridge says this for itself. An adapter which walks the reader has to say it whenever it fails
    /// with the value behind it rather than in front of it: after a nested extraction which succeeded, or once it
    /// has read its own closing token. A composite which fails part-way through a structure should finish walking
    /// that structure first -- the position inside it means nothing to a caller -- and then say so.
    ///
    /// \see skip_failed_value
    void note_value_consumed(const reader& from) noexcept;

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

    /// Where to report a problem noticed while \a from is sitting on the thing that is wrong.
    ///
    /// This is \ref path when any \ref path_scope has named a position and the reader's own \c reader::current_path
    /// when none has -- never both, since an adapter walking a single reader would otherwise have its position
    /// counted twice. \ref expect and \ref current_as report through this; an extractor which rejects a value for a
    /// reason other than its node type -- a number outside the range of what it builds, say -- wants the same answer
    /// for the same reason.
    ///
    /// It is not free: on a text-backed reader with no scope live, \c reader::current_path rescans from the start of
    /// the document. Ask for it when recording a problem, not before one happens.
    JSONV_NODISCARD
    jsonv::path problem_path(const reader& from) const;

    /// \{
    /// Attempt to extract a \c T from \a from using the \c formats associated with this context.
    ///
    /// This is the positioned primitive a composite calls for each of its parts: it extracts the value under the
    /// cursor and nothing else. In particular it does not step over \c ast_node_type::document_start, so it is not
    /// the way to start on a fresh reader -- \c jsonv::extract is.
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

    /// Attempt to extract a \c T from the in-memory \a from using the \c formats associated with this context.
    ///
    /// This runs the same pipeline as the \c reader overload by walking \a from through a \c reader::from_value, and
    /// reports failure by throwing rather than by returning. It is how an adapter written against the older
    /// \c value-based interface reaches the rest of the pipeline. To extract part of \a from, name the part --
    /// <tt>extract<T>(from.at("a"))</tt> -- under a \ref path_scope saying where it is.
    ///
    /// \throws extraction_error if anything goes wrong when attempting to extract a value.
    ///
    /// \see value_adapter_for
    template <typename T>
    JSONV_NODISCARD
    T extract(const value& from);

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

        path_scope(const path_scope&)            = delete;
        path_scope& operator=(const path_scope&) = delete;

        ~path_scope() noexcept;

    private:
        friend class extraction_context;

        /// Append this scope's ancestors and then itself to \a out, so the result reads outermost-first.
        void append_to(jsonv::path& out) const;

    private:
        extraction_context*                                       _context;
        const path_scope*                                         _parent;
        std::variant<std::size_t, std::string_view, path_element> _element;
    };

private:
    friend class path_scope;
    friend class detail::borrowed_subtree;

    friend JSONV_PUBLIC void detail::extract_entry(extraction_context&   context,
                                                   const std::type_info& type,
                                                   reader&               from,
                                                   void*                 into,
                                                   void               (* destroy)(void*) noexcept,
                                                   detail::source_lifetime lifetime
                                                  );

    /// Where to report a failure which is being translated out of an exception. Unlike \ref problem_path this takes
    /// the location a bridge left behind on its way out, because by now the cursor has moved on from the value which
    /// failed. Taking it is the point: it belongs to the failure being translated and to nothing after it.
    JSONV_NODISCARD
    jsonv::path take_failure_path(const reader& from);

private:
    extract_options   _options;
    jsonv::path       _base_path;
    const path_scope* _innermost              = nullptr;
    std::size_t       _temporary_source_depth = 0U;

    /// Where to report the failure currently unwinding, left by a bridge which walked the cursor past the value which
    /// failed. A destructor runs before the handler which records the problem, so the location has to be worked out
    /// in the destructor and picked up by \ref take_failure_path. It lives and dies with one call to \c extract.
    std::optional<jsonv::path> _failure_path;

    /// The reader whose cursor a bridge already walked past the value currently failing. Read by
    /// \ref skip_failed_value and, like \ref _failure_path, it lives and dies with one call to \c extract.
    const reader*     _consumed_failed_value = nullptr;

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
    /// Distinct from \ref _advanced: a structure read out of text is advanced by the constructor, so the two only
    /// agree for the shapes \c commit had something left to do for.
    bool                _committed;
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

    // The reader shapes consume the value themselves, so by the time `func` has returned the cursor is past it and
    // normalising -- which moves the result into the pipeline's `std::expected`, using the caller's own move
    // constructor -- is failing with that value behind it. Anything thrown *by* `func` is deliberately left alone:
    // it may have failed before consuming anything, and the call is evaluated outside the guard for that reason.
    auto normalise_consumed = [&] (auto&& raw) -> std::expected<T, ast_node_type>
                              {
                                  try
                                  {
                                      return normalise(std::forward<decltype(raw)>(raw));
                                  }
                                  catch (...)
                                  {
                                      context.note_value_consumed(from);
                                      throw;
                                  }
                              };

    if constexpr (std::invocable<const FExtract&, extraction_context&, reader&>)
    {
        return normalise_consumed(func(context, from));
    }
    else if constexpr (std::invocable<const FExtract&, reader&>)
    {
        return normalise_consumed(func(from));
    }
    else if constexpr (std::invocable<const FExtract&, extraction_context&, const value&>)
    {
        // `get()` is a `const value&`, which is the signature the `invocable` check above tested. Handing over a
        // mutable one would let a callable overloaded on both pick the other overload.
        borrowed_subtree subtree(context, from);
        auto             result = normalise(func(context, subtree.get()));
        if (!result)
            return result;

        // Committing steps the cursor past the value, so the named return -- which moves the result with the
        // caller's own move constructor -- fails with that value behind it. `borrowed_subtree` cannot say so on our
        // behalf here: it has committed, which is the state it takes to mean the body succeeded.
        subtree.commit();
        try
        {
            return result;
        }
        catch (...)
        {
            context.note_value_consumed(from);
            throw;
        }
    }
    else
    {
        static_assert(std::invocable<const FExtract&, const value&>,
                      "An extraction function must be callable as (extraction_context&, reader&), (reader&), "
                      "(extraction_context&, const value&) or (const value&)"
                     );

        borrowed_subtree subtree(context, from);
        auto             result = normalise(func(subtree.get()));
        if (!result)
            return result;

        // Committing steps the cursor past the value, so the named return -- which moves the result with the
        // caller's own move constructor -- fails with that value behind it. `borrowed_subtree` cannot say so on our
        // behalf here: it has committed, which is the state it takes to mean the body succeeded.
        subtree.commit();
        try
        {
            return result;
        }
        catch (...)
        {
            context.note_value_consumed(from);
            throw;
        }
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

/// \c extract_entry for a \c T, which also owns the storage the \c T is built in.
template <typename T>
JSONV_NODISCARD
T extract_entry(extraction_context& context, reader& from, source_lifetime lifetime)
{
    alignas(T) std::byte place[sizeof(T)];
    extract_entry(context,
                  typeid(T),
                  from,
                  static_cast<void*>(place),
                  [](void* p) noexcept { std::destroy_at(std::launder(static_cast<T*>(p))); },
                  lifetime
                 );

    T*   ptr     = std::launder(reinterpret_cast<T*>(place));
    auto destroy = on_scope_exit([ptr] { std::destroy_at(ptr); });
    return std::move(*ptr);
}

/// Extract a \c T from JSON \a source text.
///
/// A \c std::string rvalue is taken over: it is moved into a reader which lives as long as this call, so the extraction
/// is told its source is temporary and refuses to hand back views of it. Anything else is read where it is, through a
/// \c std::string_view, and views of it are the caller's to keep valid -- that includes an rvalue of any other string
/// type, such as a \c std::pmr::string, which outlives this call as every temporary argument does.
template <typename T, typename TSource>
JSONV_NODISCARD
T extract_text(TSource&&              source,
               const parse_options&   parse_opts,
               const formats&         fmts,
               const extract_options& options
              )
{
    extraction_context context(fmts, std::nullopt, jsonv::path(), nullptr, options);
    if constexpr (std::is_same_v<TSource, std::string>)
    {
        reader from(std::move(source), parse_opts);
        return extract_entry<T>(context, from, source_lifetime::extraction);
    }
    else
    {
        // Forwarded, so the conversion used is the one the constraint on the entry point accepted: a type may convert
        // to text only as an rvalue, or differently as an lvalue and as an rvalue.
        reader from(std::string_view(std::forward<TSource>(source)), parse_opts);
        return extract_entry<T>(context, from, source_lifetime::caller);
    }
}

}

template <typename T>
T extraction_context::extract(const value& from)
{
    reader rdr = reader::from_value(from);
    return detail::extract_entry<T>(*this, rdr, detail::source_lifetime::caller);
}

/// Extract a C++ value from \a from using the provided \a fmts.
template <typename T>
JSONV_NODISCARD
T extract(const value& from, const formats& fmts)
{
    extraction_context context(fmts);
    return context.extract<T>(from);
}

/// Extract a C++ value from \a from using the provided \a fmts and \a options.
template <typename T>
JSONV_NODISCARD
T extract(const value& from, const formats& fmts, const extract_options& options)
{
    extraction_context context(fmts, std::nullopt, jsonv::path(), nullptr, options);
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

/// Extract a C++ value from \a from using \c jsonv::formats::global() and the provided \a options.
template <typename T>
JSONV_NODISCARD
T extract(const value& from, const extract_options& options)
{
    extraction_context context(formats::global(), std::nullopt, jsonv::path(), nullptr, options);
    return context.extract<T>(from);
}

/// \{
/// Extract a C++ value from a \a reader using \a fmts (by default \c jsonv::formats::global()) and \a options.
///
/// A reader on \c ast_node_type::document_start -- a freshly-created one -- is read as a whole document. It is checked
/// with \c reader::validate first, so a source which did not parse is reported as that rather than as whatever an
/// extractor made of the \c ast_node_type::error node it ran into; its \c document_start is stepped over, so neither
/// the caller nor any \c extractor has to; and the value read must be the whole document, so the reader is left on
/// \c ast_node_type::document_end. A reader the caller has already positioned gets none of this: the value under its
/// cursor is extracted and the cursor left one past it, as \c reader::next_value would, whatever surrounds it. The
/// exception is a reader on an \c ast_node_type::error node, which has no value to extract and is reported as the
/// parse failure it is.
///
/// Anything extracted as a view of the source -- a \c std::string_view -- views the reader's storage. Through the
/// rvalue overloads, a reader which \c reader::owns_source dies with the call, so such views are refused; one over
/// storage the caller owns, like <tt>reader(std::string_view)</tt>, is viewed as usual.
///
/// \throws extraction_error if the source did not parse, the value could not be extracted or, for a whole document,
///                          something follows the value.
template <typename T>
JSONV_NODISCARD
T extract(reader& from, const formats& fmts = formats::global(), const extract_options& options = extract_options())
{
    extraction_context context(fmts, std::nullopt, jsonv::path(), nullptr, options);
    return detail::extract_entry<T>(context, from, detail::source_lifetime::caller);
}

/// Extract a C++ value from a \a reader using \c jsonv::formats::global() and the provided \a options.
template <typename T>
JSONV_NODISCARD
T extract(reader& from, const extract_options& options)
{
    return extract<T>(from, formats::global(), options);
}

/// Extract a C++ value from a \a reader which may own its source, using \a fmts and \a options.
template <typename T>
JSONV_NODISCARD
T extract(reader&& from, const formats& fmts = formats::global(), const extract_options& options = extract_options())
{
    extraction_context context(fmts, std::nullopt, jsonv::path(), nullptr, options);
    return detail::extract_entry<T>(context,
                                    from,
                                    from.owns_source() ? detail::source_lifetime::extraction
                                                       : detail::source_lifetime::caller
                                   );
}

/// Extract a C++ value from a \a reader which may own its source, using \c jsonv::formats::global() and the provided
/// \a options.
template <typename T>
JSONV_NODISCARD
T extract(reader&& from, const extract_options& options)
{
    return extract<T>(std::move(from), formats::global(), options);
}
/// \}

/// \{
/// Extract a C++ value directly from JSON \a source text, parsed with \a parse_opts, using \a fmts (by default
/// \c jsonv::formats::global()) and \a options.
///
/// \a source is anything which converts to \c std::string_view: a string literal, a \c std::string, a
/// \c std::string_view. Note what that means for a C++ string: it is JSON text to be parsed, not a JSON string, so
/// <tt>extract<std::string>(R"("fire")")</tt> is \c "fire" and <tt>extract<std::string>("fire")</tt> is a parse
/// failure. Wrap it in a \c value -- <tt>extract<std::string>(value("fire"))</tt> -- to mean the string.
///
/// A \c std::string rvalue is taken over for the call and freed when it returns, so views of it are refused, as for a
/// tree materialised during extraction (see \c extraction_context::source_is_temporary). Every other source is read
/// where it is, without copying, and a \c std::string_view extracted from it points into it.
///
/// This reads the whole document, exactly as the \c reader overloads do given a fresh \c reader.
///
/// \throws extraction_error if \a source is not valid JSON, the value could not be extracted, or something follows it.
/// \throws std::invalid_argument if \a parse_opts asks for a \c parse_options::max_structure_depth beyond the limit,
///                               as \c jsonv::parse does.
template <typename T, typename TSource>
    requires std::convertible_to<TSource, std::string_view>
JSONV_NODISCARD
T extract(TSource&&              source,
          const formats&         fmts    = formats::global(),
          const extract_options& options = extract_options()
         )
{
    return detail::extract_text<T>(std::forward<TSource>(source), parse_options::create_default(), fmts, options);
}

/// Extract a C++ value from JSON \a source text using \c jsonv::formats::global() and the provided \a options.
template <typename T, typename TSource>
    requires std::convertible_to<TSource, std::string_view>
JSONV_NODISCARD
T extract(TSource&& source, const extract_options& options)
{
    return detail::extract_text<T>(std::forward<TSource>(source),
                                   parse_options::create_default(),
                                   formats::global(),
                                   options
                                  );
}

/// Extract a C++ value from JSON \a source text parsed with \a parse_opts, using \a fmts and \a options.
template <typename T, typename TSource>
    requires std::convertible_to<TSource, std::string_view>
JSONV_NODISCARD
T extract(TSource&&              source,
          const parse_options&   parse_opts,
          const formats&         fmts    = formats::global(),
          const extract_options& options = extract_options()
         )
{
    return detail::extract_text<T>(std::forward<TSource>(source), parse_opts, fmts, options);
}

/// Extract a C++ value from JSON \a source text parsed with \a parse_opts, using \c jsonv::formats::global() and the
/// provided \a options.
template <typename T, typename TSource>
    requires std::convertible_to<TSource, std::string_view>
JSONV_NODISCARD
T extract(TSource&& source, const parse_options& parse_opts, const extract_options& options)
{
    return detail::extract_text<T>(std::forward<TSource>(source), parse_opts, formats::global(), options);
}
/// \}

/// \}

}
