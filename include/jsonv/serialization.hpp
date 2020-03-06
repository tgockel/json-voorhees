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

/// The action to take when an insertion of an extractor or serializer into a formats is attempted, but there is alredy
/// an extractor or serializer for that type.
enum class duplicate_type_action : unsigned char
{
    /// The existing extractor or serializer should be kept, but no exception should be thrown.
    ignore,
    /// The new extractor or serializer should be inserted, and no exception should be thrown.
    replace,
    /// A \ref duplicate_type_error should be thrown.
    exception,
};

/// Exception thrown if an insertion of an extractor or serializer into a formats is attempted, but there is already an
/// extractor or serializer for that type.
class JSONV_PUBLIC duplicate_type_error :
        public std::invalid_argument
{
public:
    explicit duplicate_type_error(const std::string& operation, const std::type_index& type);

    virtual ~duplicate_type_error() noexcept override;

    const std::type_index& type_index() const { return _type_index; }

private:
    std::type_index _type_index;
};

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
        const std::string& message() const noexcept
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

/// Thrown when \c formats::extract does not have an \c extractor for the provided type.
class JSONV_PUBLIC no_extractor :
        public std::runtime_error
{
public:
    /// \{
    /// Create a new exception.
    explicit no_extractor(const std::type_info& type);
    explicit no_extractor(const std::type_index& type);
    /// \}

    virtual ~no_extractor() noexcept;

    /// The name of the type.
    string_view type_name() const;

    /// Get an ID for the type of \c extractor that \c formats::extract could not locate.
    std::type_index type_index() const;

private:
    std::type_index _type_index;
    std::string     _type_name;
};

/** Thrown when \c formats::to_json does not have a \c serializer for the provided type. **/
class JSONV_PUBLIC no_serializer :
        public std::runtime_error
{
public:
    /** Create a new exception. **/
    explicit no_serializer(const std::type_info& type);
    explicit no_serializer(const std::type_index& type);

    virtual ~no_serializer() noexcept;

    /** The name of the type. **/
    string_view type_name() const;

    /** Get an ID for the type of \c serializer that \c formats::to_json could not locate. **/
    std::type_index type_index() const;

private:
    std::type_index _type_index;
    std::string     _type_name;
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

/// An \c extractor holds the method for converting JSON source into an arbitrary C++ type.
class JSONV_PUBLIC extractor
{
public:
    virtual ~extractor() noexcept;

    /// Get the run-time type this \c extractor knows how to extract. Once this \c extractor is registered with a
    /// \c formats, it is not allowed to change.
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

/** A \c serializer holds the method for converting an arbitrary C++ type into a \c value. **/
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

/** An \c adapter is both an \c extractor and a \c serializer. It is made with the idea that for \e most types, you want
 *  to both encode and decode JSON.
**/
class JSONV_PUBLIC adapter :
        public extractor,
        public serializer
{
public:
    virtual ~adapter() noexcept;
};

/// Simply put, this class is a collection of \c extractor and \c serializer instances.
///
/// Ultimately, \c formats form a directed graph of possible types to load. This allows you to compose formats including
/// your application with any number of 3rd party libraries an base formats. Imagine an application that has both a user
/// facing API and an object storage system, both of which speak JSON. When loading from the database, you would like
/// strict type enforcement -- check that when you say <tt>extract&lt;int&gt;(val)</tt>, that \c val actually has
/// \c kind::integer. When getting values from the API, you happily convert \c kind::string with the value \c "51" into
/// the integer \c 51. You really don't want to have to write two versions of decoders for all of your objects: one
/// which uses the standard checked \c value::as_integer and one that uses \c coerce_integer -- you want the same object
/// model.
///
/// \dot
/// digraph formats {
///   defaults  [label="formats::defaults"]
///   coerce    [label="formats::coerce"]
///   lib       [label="some_3rd_party"]
///   my_app    [label="my_app_formats"]
///   db        [label="my_app_database_loader"]
///   api       [label="my_app_api_loader"]
///
///   my_app -> lib
///   db ->  defaults
///   db ->  my_app
///   api -> coerce
///   api -> my_app
/// }
/// \enddot
///
/// To do this, you would create your object model (called \c my_app_formats in the chart) with the object models for
/// your application-specific types. This can use any number of 3rd party libraries to get the job done. To make a
/// functional \c formats, you would \c compose different \c formats instances into a single one. From there,
///
/// \code
/// jsonv::formats get_api_formats()
/// {
///     static jsonv::formats instance = jsonv::formats::compose({ jsonv::formats::coerce(), get_app_formats() });
///     return instance;
/// }
///
/// jsonv::formats get_db_formats()
/// {
///     static jsonv::formats instance = jsonv::formats::compose({ jsonv::formats::defaults(), get_app_formats() });
///     return instance;
/// }
///
/// MyType extract_thing(const jsonv::value& from, bool from_db)
/// {
///     return jsonv::extract<MyType>(from, from_db ? get_db_formats() : get_api_formats());
/// }
/// \endcode
class JSONV_PUBLIC formats
{
public:
    using list = std::vector<formats>;

public:
    /** Get the default \c formats instance. This uses \e strict type-checking and behaves by the same rules as the
     *  \c value \c as_ member functions (\c as_integer, \c as_string, etc).
     *
     *  \note
     *  This function actually returns a \e copy of the default \c formats, so modifications do not affect the actual
     *  instance.
    **/
    static formats defaults();

    /** Get the global \c formats instance. By default, this is the same as \c defaults, but you can override it with
     *  \c set_global. The \c extract function uses this \c formats instance if none is provided, so this is convenient
     *  if your application only has one type of \c formats to use.
     *
     *  \note
     *  This function actually returns a \e copy of the global \c formats, so modifications do not affect the actual
     *  instance. If you wish to alter the global formats, use \c set_global.
    **/
    static formats global();

    /** Set the \c global \c formats instance.
     *
     *  \returns the previous value of the global formats instance.
    **/
    static formats set_global(formats);

    /** Reset the \c global \c formats instance to \c defaults.
     *
     *  \returns the previous value of the global formats instance.
    **/
    static formats reset_global();

    /** Get the coercing \c formats instance. This uses \e loose type-checking and behaves by the same rules as the
     *  \c coerce_ functions in \c coerce.hpp.
     *
     *  \note
     *  This function actually returns a \e copy of the default \c formats, so modifications do not affect the actual
     *  instance.
    **/
    static formats coerce();

    /** Create a new, empty \c formats instance. By default, this does not know how to extract anything -- not even the
     *  basic types like \c int64_t or \c std::string.
    **/
    formats();

    ~formats() noexcept;

    /** Create a new (empty) \c formats using the \a bases as backing \c formats. This forms a directed graph of
     *  \c formats objects. When searching for an \c extractor or \c serializer, the \c formats is searched, then each
     *  base is searched depth-first left-to-right.
     *
     *  \param bases Is the list of \c formats objects to use as bases for the newly-created \c formats. Order matters
     *               -- the \a bases are searched left-to-right, so \c formats that are farther left take precedence
     *               over those more to the right.
     *
     *  \note
     *  It is impossible to form an endless loop of \c formats objects, since the base of all \c formats are eventually
     *  empty. If there is an existing set of nodes \f$ k \f$ and each new \c format created with \c compose is in
     *  \f$ k+1 \f$, there is no way to create a link from \f$ k \f$ into \f$ k+1 \f$ and so there is no way to create a
     *  circuit.
    **/
    static formats compose(const list& bases);

    /** Extract the provided \a type \a from a \c value \a into an area of memory. The \a context is passed to the
     *  \c extractor which performs the conversion. In general, this should not be used directly as it is quite painful
     *  to do so -- prefer \c extraction_context::extract or the free function \c jsonv::extract.
     *
     *  \throws no_extractor if an \c extractor for \a type could not be found.
    **/
    void extract(const std::type_info&     type,
                 const value&              from,
                 void*                     into,
                 const extraction_context& context
                ) const;

    /** Get the \c extractor for the given \a type.
     *
     *  \throws no_extractor if an \c extractor for \a type could not be found.
    **/
    const extractor& get_extractor(std::type_index type) const;

    /** Get the \c extractor for the given \a type.
     *
     *  \throws no_extractor if an \c extractor for \a type could not be found.
    **/
    const extractor& get_extractor(const std::type_info& type) const;

    /** Encode the provided value \a from into a JSON \c value. The \a context is passed to the \c serializer which
     *  performs the conversion. In general, this should not be used directly as it is painful to do so -- prefer
     *  \c serialization_context::to_json or the free function \c jsonv::to_json.
     *
     *  \throws no_serializer if a \c serializer for \a type could not be found.
    **/
    value to_json(const std::type_info&        type,
                  const void*                  from,
                  const serialization_context& context
                 ) const;

    /** Gets the \c serializer for the given \a type.
     *
     *  \throws no_serializer if a \c serializer for \a type could not be found.
    **/
    const serializer& get_serializer(std::type_index type) const;

    /** Gets the \c serializer for the given \a type.
     *
     *  \throws no_serializer if a \c serializer for \a type could not be found.
    **/
    const serializer& get_serializer(const std::type_info& type) const;

    /** Register an \c extractor that lives in some unmanaged space.
     *
     *  \throws duplicate_type_error if this \c formats instance already has an \c extractor that serves the provided
     *                               \c extractor::get_type and the \c duplicate_type_action is \c exception.
    **/
    void register_extractor(const extractor*, duplicate_type_action action = duplicate_type_action::exception);

    /** Register an \c extractor with shared ownership between this \c formats instance and anything else.
     *
     *  \throws duplicate_type_error if this \c formats instance already has an \c extractor that serves the provided
     *                               \c extractor::get_type and the \c duplicate_type_action is \c exception.
    **/
    void register_extractor(std::shared_ptr<const extractor>,
                            duplicate_type_action action = duplicate_type_action::exception);

    /** Register a \c serializer that lives in some managed space.
     *
     *  \throws duplicate_type_error if this \c formats instance already has a \c serializer that serves the provided
     *                               \c serializer::get_type and the \c duplicate_type_action is \c exception.
    **/
    void register_serializer(const serializer*, duplicate_type_action action = duplicate_type_action::exception);

    /** Register a \c serializer with shared ownership between this \c formats instance and anything else.
     *
     *  \throws duplicate_type_error if this \c formats instance already has a \c serializer that serves the provided
     *                               \c serializer::get_type and the \c duplicate_type_action is \c exception.
    **/
    void register_serializer(std::shared_ptr<const serializer>,
                             duplicate_type_action action = duplicate_type_action::exception);

    /** Register an \c adapter that lives in some unmanaged space.
     *
     *  \throws duplicate_type_error if this \c formats instance already has either an \c extractor or \c serializer
     *                               that serves the provided \c adapter::get_type and the \c duplicate_type_action is
     *                               \c exception.
    **/
    void register_adapter(const adapter*, duplicate_type_action action = duplicate_type_action::exception);

    /** Register an \c adapter with shared ownership between this \c formats instance and anything else.
     *
     *  \throws duplicate_type_error if this \c formats instance already has either an \c extractor or \c serializer
     *                               that serves the provided \c adapter::get_type the \c duplicate_type_action is
     *                               \c exception.
    **/
    void register_adapter(std::shared_ptr<const adapter>,
                          duplicate_type_action action = duplicate_type_action::exception);

    /** Test for equality between this instance and \a other. If two \c formats are equal, they are the \e exact same
     *  node in the graph. Even if one \c formats has the exact same types for the exact same <tt>extractor</tt>s.
    **/
    bool operator==(const formats& other) const;

    /** Test for inequality between this instance and \a other. The opposite of \c operator==. **/
    bool operator!=(const formats& other) const;

private:
    struct data;

private:
    explicit formats(std::vector<std::shared_ptr<const data>> bases);

private:
    std::shared_ptr<data> _data;
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

class JSONV_PUBLIC extraction_context :
        public context_base
{
public:
    /** Create a new instance using the default \c formats (\c formats::global). **/
    extraction_context();

    /** Create a new instance using the given \a fmt, \a ver and \a p. **/
    explicit extraction_context(jsonv::formats        fmt,
                                const jsonv::version& ver      = jsonv::version(),
                                jsonv::path           p        = jsonv::path(),
                                const void*           userdata = nullptr
                               );

    virtual ~extraction_context() noexcept;

    /** Get the current \c path this \c extraction_context is extracting for. This is useful when debugging and
     *  generating error messages.
    **/
    const jsonv::path& path() const
    {
        return _path;
    }

    /** Attempt to extract a \c T from \a from using the \c formats associated with this context.
     *
     *  \tparam T is the type to extract from \a from. It must be movable.
     *
     *  \throws extraction_error if anything goes wrong when attempting to extract a value.
    **/
    template <typename T>
    T extract(const value& from) const
    {
        typename std::aligned_storage<sizeof(T), alignof(T)>::type place[1];
        T* ptr = reinterpret_cast<T*>(place);
        extract(typeid(T), from, static_cast<void*>(ptr));
        auto destroy = detail::on_scope_exit([ptr] { ptr->~T(); });
        return std::move(*ptr);
    }

    void extract(const std::type_info&     type,
                 const value&              from,
                 void*                     into
                ) const;

    /** Attempt to extract a \c T from <tt>from.at_path(subpath)</tt> using the \c formats associated with this context.
     *
     *  \tparam T is the type to extract from \a from. It must be movable.
     *
     *  \throws extraction_error if anything goes wrong when attempting to extract a value.
    **/
    template <typename T>
    T extract_sub(const value& from, jsonv::path subpath) const
    {
        typename std::aligned_storage<sizeof(T), alignof(T)>::type place[1];
        T* ptr = reinterpret_cast<T*>(place);
        extract_sub(typeid(T), from, std::move(subpath), static_cast<void*>(ptr));
        auto destroy = detail::on_scope_exit([ptr] { ptr->~T(); });
        return std::move(*ptr);
    }

    void extract_sub(const std::type_info& type, const value& from, jsonv::path subpath, void* into) const;

    /** Attempt to extract a \c T from <tt>from.at_path({elem})</tt> using the \c formats associated with this context.
     *
     *  \tparam T is the type to extract from \a from. It must be movable.
     *
     *  \throws extraction_error if anything goes wrong when attempting to extract a value.
    **/
    template <typename T>
    T extract_sub(const value& from, path_element elem) const
    {
        return extract_sub<T>(from, jsonv::path({ elem }));
    }

private:
    jsonv::path _path;
};

/// Extract a C++ value from \a from using the provided \a fmts.
template <typename T>
T extract(const value& from, const formats& fmts)
{
    extraction_context context(fmts);
    return context.extract<T>(from);
}

/// Extract a C++ value from \a from using \c jsonv::formats::global().
template <typename T>
T extract(const value& from)
{
    extraction_context context;
    return context.extract<T>(from);
}

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
