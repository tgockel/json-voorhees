/// \file jsonv/reader.hpp
/// Read a JSON AST.
///
/// Copyright (c) 2015-2022 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/ast.hpp>
#include <jsonv/string_view.hpp>

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>

namespace jsonv
{

class parse_index;
class parse_options;
class path;
class value;

/// \ingroup Serialization
/// \{

/// A reader instance reads from some form of JSON source (probably a string) and converts it into a JSON \ref ast_node
/// sequence.
///
/// Readers normalize access to JSON source for conversion to some other format. They can be provided with pre-parsed
/// JSON through a \c parse_index or \c value. They can be provided with a \c std::string or \c string_view directly.
/// This allows \c extractor implementations to operate on all forms of JSON without worrying about the implementation.
class JSONV_PUBLIC reader final
{
public:
    /// Create a reader which reads from the given \a index.
    explicit reader(parse_index index);

    /// Create a reader which reads from an in-memory \a value.
    ///
    /// \param value The value to read from. This must remain valid for the lifetime of the reader.
    explicit reader(const value* value);

    /// \{
    /// Create a reader which reads from JSON \a source.
    ///
    /// \param source The JSON source code to parse from. This must stay in memory for the duration of this instance's
    ///               use. If source is an rvalue reference to a \c std::string instance, the \a source is moved to the
    ///               reader's implementation to keep alive.
    /// \param parse_options If specified, use these options to parse \a source. If unspecified, use the default values
    ///                      of \a parse_options for \c parse_index::parse.
    explicit reader(string_view source);
    explicit reader(string_view source, const parse_options& parse_options);
    explicit reader(const char* source);
    explicit reader(const char* source, const parse_options& parse_options);

    explicit reader(std::string&& source);
    explicit reader(std::string&& source, const parse_options& parse_options);
    /// \}

    // Not copyable.
    reader(const reader&)            = delete;
    reader& operator=(const reader&) = delete;

    reader(reader&&) noexcept            = default;
    reader& operator=(reader&&) noexcept = default;

    ~reader() noexcept;

    /// Check if this reader is still good to read from. This will be \c true if this instance has not been moved-from
    /// and has not reached EOF. If this is \c false, \c current or \c current_path will throw an exception.
    bool good() const;

    /// Get the current AST node this reader is pointing at.
    ///
    /// \throws std::invalid_argument if this instance is not \c good.
    const ast_node& current() const;

    /// \{
    /// Check that the \c current AST node has the given \a type or is one of the expected \a types. Using this leads to
    /// a slightly more informative error message than `current.as<T>()` call.
    ///
    /// \throws extraction_error if the current node does not match the expected \a type or \a types.
    /// \throws std::invalid_argument if this instance is not \c good.
    void expect(ast_node_type type);
    void expect(std::initializer_list<ast_node_type> types);
    /// \}

    /// Get the \c current AST node as a specific \c TAstNode subtype, calling \c expect beforehand.
    ///
    /// \throws std::invalid_argument if this instance is not \c good.
    /// \throws extraction_error if the current node does not match `TAstNode::type()`.
    template <typename TAstNode>
    TAstNode current_as() const
    {
        expect(TAstNode::type());
        return current().as<TAstNode>();
    }

    /// Get the path to the current node this reader is pointing at. This is used in the generation of error messages to
    /// describe the location of something that could not be extracted.
    ///
    /// \code
    /// ^               /* "."  -- start of document is the empty path */
    /// {               /* "."  -- opening { is still an empty path */
    ///   "a":          /* ".a" -- the key starts the path */
    ///     [           /* ".a" -- the path refers to the entire array */
    ///       1,        /* ".a[0]" */
    ///       2,        /* ".a[1]" */
    ///       3,        /* ".a[2]" */
    ///     ],          /* ".a" -- the path at the end of the array refers to the entire array again */
    ///   "b":          /* ".b" */
    ///     {           /* ".b" -- the path refers to the entire object */
    ///       "x":      /* ".b.x" */
    ///         "taco"  /* ".b.x" */
    ///     },          /* ".b" */
    ///   "c":          /* ".c" */
    ///     4           /* ".c" */
    /// }               /* "." */
    /// $               /* "." */
    /// \endcode
    ///
    /// \throws std::invalid_argument if this instance is not \c good.
    const path& current_path() const;

    /// Go to the next token.
    ///
    /// \code
    /// ^
    /// {        /* <- go to "a" */
    ///   "a":   /* <- go to [ */
    ///     [    /* <- go to 1 */
    ///       1, /* <- go to 2 */
    ///       2, /* ...and so on */
    ///       3,
    ///     ],
    ///   "b":
    ///     {
    ///     },
    ///   "c":
    ///     4
    /// }
    /// $
    /// \endcode
    ///
    /// \returns \c true if the reader is still \c good to read from \c current.
    [[nodiscard]]
    bool next_token() noexcept;

    /// Go to one past the end of the current structure.
    ///
    /// \code
    /// ^
    /// {
    ///   "a":   /* <- go to end of document */
    ///     [    /* <- go to "b" */
    ///       1, /* <- go to "b", too */
    ///       2,
    ///       3,
    ///     ],   /* <- go to "b" */
    ///   "b":   /* <- go to end of document */
    ///     {    /* <- go to "c" */
    ///     },   /* <- go to "c" */
    ///   "c":
    ///     4
    /// }        /* <- go to end of document */
    /// $
    /// \endcode
    ///
    /// This is meant to be used when parsing an object or array
    ///
    /// \code
    /// my_object extract_my_object(jsonv::reader& from)
    /// {
    ///     from.expect(jsonv::ast_node_type::object_begin);
    ///     from.next_token();
    ///
    ///     // Use this helper macro to always execute code
    ///     JSONV_SCOPE_EXIT { from.next_structure(); };
    ///
    ///     std::optional<std::int64_t> a;
    ///     while (from.good())
    ///     {
    ///         auto token = from.current();
    ///         if (token.type() == jsonv::ast_node_type::object_end)
    ///         {
    ///             return my_object(a.value_or(0));
    ///         }
    ///         else if (token.type() == jsonv::ast_node_type::key_canonical)
    ///         {
    ///             if (token.value() == "a")
    ///             {
    ///                 reader.next_token();
    ///                 a = reader.current_as<jsonv::ast_node::integer>().value();
    ///                 reader.next_token();
    ///             }
    ///             else
    ///             {
    ///                 // ignore
    ///                 reader.next_key();
    ///             }
    ///         }
    ///         else
    ///         {
    ///             throw jsonv::extraction_error(from.current_path(), "Did not handle escaped keys");
    ///         }
    ///     }
    /// }
    /// \endcode
    ///
    /// \returns \c true if the reader is still \c good to read from \c current.
    [[nodiscard]]
    bool next_structure() noexcept;

    /// Go to the next object key or end-of-object.
    ///
    /// \code
    /// ^
    /// {
    ///   "a":  /* <- calling here goes to "b" */
    ///     [   /* <- calling when not on an object key throws */
    ///       1,
    ///       2,
    ///       3,
    ///     ],
    ///   "b":  /* <- calling here goes to "c" */
    ///     {
    ///     },
    ///   "c":  /* <- calling here goes to end of object */
    ///     4
    /// }
    /// $
    /// \endcode
    ///
    /// \returns \c true if the reader is still \c good to read from \c current.
    /// \throws std::invalid_argument if the reader is not currently at the start of a key.
    [[nodiscard]]
    bool next_key() noexcept;

private:
    class impl;
    class impl_parse_index;
    class impl_parse_index_owning;
    class impl_value;

    template <typename TImpl, typename... TArgs>
    explicit reader(std::in_place_type_t<TImpl>, TArgs&&...);

private:
    std::unique_ptr<impl> _impl;
};

/// \}

}
