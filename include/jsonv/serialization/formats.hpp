/// \file jsonv/serialization/formats.hpp
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
#include <jsonv/string_view.hpp>

#include <memory>
#include <stdexcept>
#include <typeindex>
#include <vector>

namespace jsonv
{

class adapter;
class extractor;
class extraction_context;
class serializer;
class serialization_context;
class value;

/// \addtogroup Serialization
/// \{

/// The action to take when an insertion of an extractor or serializer into a \c formats is attempted, but there is
/// already an extractor or serializer for that type.
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
    explicit duplicate_type_error(string_view operation, const std::type_index& type);

    virtual ~duplicate_type_error() noexcept override;

    /// The type
    const std::type_index& type_index() const
    {
        return _type_index;
    }

private:
    std::type_index _type_index;
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
/// functional \c formats, you would \c compose different \c formats instances into a single one.
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
class JSONV_PUBLIC formats final
{
public:
    using list = std::vector<formats>;

public:
    /// Get the default \c formats instance. This uses \e strict type-checking and behaves by the same rules as the
    /// \c value \c as_ member functions (\c as_integer, \c as_string, etc).
    ///
    /// \note
    /// This function actually returns a \e copy of the default \c formats, so modifications do not affect the actual
    /// instance.
    static formats defaults();

    /// Get the global \c formats instance. By default, this is the same as \c defaults, but you can override it with
    /// \c set_global. The \c extract function uses this \c formats instance if none is provided, so this is convenient
    /// if your application only has one type of \c formats to use.
    ///
    /// \note
    /// This function actually returns a \e copy of the global \c formats, so modifications do not affect the actual
    /// instance. If you wish to alter the global formats, use \c set_global.
    static formats global();

    /// Set the \c global \c formats instance.
    ///
    /// \returns the previous value of the global formats instance.
    static formats set_global(formats);

    /// Reset the \c global \c formats instance to \c defaults.
    ///
    /// \returns the previous value of the global formats instance.
    static formats reset_global();

    /// Get the coercing \c formats instance. This uses \e loose type-checking and behaves by the same rules as the
    /// \c coerce_ functions in \c coerce.hpp.
    ///
    /// \note
    /// This function actually returns a \e copy of the default \c formats, so modifications do not affect the actual
    /// instance.
    static formats coerce();

    /// Create a new, empty \c formats instance. By default, this does not know how to extract anything -- not even the
    /// basic types like \c int64_t or \c std::string.
    formats();

    ~formats() noexcept;

    /// Create a new (empty) \c formats using the \a bases as backing \c formats. This forms a directed graph of
    /// \c formats objects. When searching for an \c extractor or \c serializer, the \c formats is searched, then each
    /// base is searched depth-first left-to-right.
    ///
    /// \param bases Is the list of \c formats objects to use as bases for the newly-created \c formats. Order matters
    ///              -- the \a bases are searched left-to-right, so \c formats that are farther left take precedence
    ///              over those more to the right.
    ///
    /// \note
    /// It is impossible to form an endless loop of \c formats objects, since the base of all \c formats are eventually
    /// empty. If there is an existing set of nodes \f$ k \f$ and each new \c format created with \c compose is in
    /// \f$ k+1 \f$, there is no way to create a link from \f$ k \f$ into \f$ k+1 \f$ and so there is no way to create a
    /// circuit.
    static formats compose(const list& bases);

    /// Get the \c extractor for the given \a type.
    ///
    /// \throws no_extractor if an \c extractor for \a type could not be found.
    const extractor& get_extractor(std::type_index type) const;

    /// Get the \c extractor for the given \a type.
    ///
    /// \throws no_extractor if an \c extractor for \a type could not be found.
    const extractor& get_extractor(const std::type_info& type) const;

    /// Encode the provided value \a from into a JSON \c value. The \a context is passed to the \c serializer which
    /// performs the conversion. In general, this should not be used directly as it is painful to do so -- prefer
    /// \c serialization_context::to_json or the free function \c jsonv::to_json.
    ///
    /// \throws no_serializer if a \c serializer for \a type could not be found.
    value to_json(const std::type_info&        type,
                  const void*                  from,
                  const serialization_context& context
                 ) const;

    /// Gets the \c serializer for the given \a type.
    ///
    /// \throws no_serializer if a \c serializer for \a type could not be found.
    const serializer& get_serializer(std::type_index type) const;

    /// Gets the \c serializer for the given \a type.
    ///
    /// \throws no_serializer if a \c serializer for \a type could not be found.
    const serializer& get_serializer(const std::type_info& type) const;

    /// Register an \c extractor that lives in some unmanaged space.
    ///
    /// \throws duplicate_type_error if this \c formats instance already has an \c extractor that serves the provided
    ///                              \c extractor::get_type and the \c duplicate_type_action is \c exception.
    void register_extractor(const extractor*, duplicate_type_action action = duplicate_type_action::exception);

    /// Register an \c extractor with shared ownership between this \c formats instance and anything else.
    ///
    /// \throws duplicate_type_error if this \c formats instance already has an \c extractor that serves the provided
    ///                              \c extractor::get_type and the \c duplicate_type_action is \c exception.
    void register_extractor(std::shared_ptr<const extractor>,
                            duplicate_type_action action = duplicate_type_action::exception
                           );

    /// Register a \c serializer that lives in some managed space.
    ///
    /// \throws duplicate_type_error if this \c formats instance already has a \c serializer that serves the provided
    ///                              \c serializer::get_type and the \c duplicate_type_action is \c exception.
    void register_serializer(const serializer*, duplicate_type_action action = duplicate_type_action::exception);

    /// Register a \c serializer with shared ownership between this \c formats instance and anything else.
    ///
    /// \throws duplicate_type_error if this \c formats instance already has a \c serializer that serves the provided
    ///                              \c serializer::get_type and the \c duplicate_type_action is \c exception.
    void register_serializer(std::shared_ptr<const serializer>,
                             duplicate_type_action action = duplicate_type_action::exception);

    /// Register an \c adapter that lives in some unmanaged space.
    ///
    /// \throws duplicate_type_error if this \c formats instance already has either an \c extractor or \c serializer
    ///                              that serves the provided \c adapter::get_type and the \c duplicate_type_action is
    ///                              \c exception.
    void register_adapter(const adapter*, duplicate_type_action action = duplicate_type_action::exception);

    /// Register an \c adapter with shared ownership between this \c formats instance and anything else.
    ///
    /// \throws duplicate_type_error if this \c formats instance already has either an \c extractor or \c serializer
    ///                              that serves the provided \c adapter::get_type the \c duplicate_type_action is
    ///                              \c exception.
    void register_adapter(std::shared_ptr<const adapter>,
                          duplicate_type_action action = duplicate_type_action::exception
                         );

    /// Test for equality between this instance and \a other. If two \c formats are equal, they are the \e exact same
    /// node in the graph. Even if one \c formats has the exact same types for the exact same <tt>extractor</tt>s.
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

/// Thrown when \c formats::extract does not have an \c extractor for the provided type.
class JSONV_PUBLIC no_extractor :
        public std::runtime_error
{
public:
    /// \{
    /// Create a new exception noting the given \a type.
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
    /// \{
    /// Create a new exception noting the given \a type.
    explicit no_serializer(const std::type_info& type);
    explicit no_serializer(const std::type_index& type);
    /// \}

    virtual ~no_serializer() noexcept;

    /// The name of the type.
    string_view type_name() const;

    /// Get an ID for the type of \c serializer that \c formats::to_json could not locate.
    std::type_index type_index() const;

private:
    std::type_index _type_index;
    std::string     _type_name;
};

/// \}

}
