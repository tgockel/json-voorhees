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
#include <expected>
#include <string_view>

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
/// JSON through a \c parse_index or \c value. They can be provided with a \c std::string or \c std::string_view directly.
/// This allows \c extractor implementations to operate on all forms of JSON without worrying about the implementation.
///
/// A reader is a forward cursor over that sequence. It starts on \c ast_node_type::document_start, so the first thing
/// to do is step onto the value itself. Reading an object means walking its keys, handling the ones you recognize and
/// skipping the ones you do not:
///
/// \code
/// struct my_object
/// {
///     std::int64_t a = 0;
/// };
///
/// std::optional<my_object> extract_my_object(jsonv::reader& from)
/// {
///     if (!from.expect(jsonv::ast_node_type::object_begin))
///         return std::nullopt;
///
///     // Step onto the first key, or onto the } of an empty object.
///     if (!from.next_token())
///         return std::nullopt;
///
///     my_object out;
///     while (from.good() && from.current().type() != jsonv::ast_node_type::object_end)
///     {
///         // Keys arrive canonical or escaped, depending on whether the source used escape sequences.
///         if (!from.expect({ jsonv::ast_node_type::key_canonical, jsonv::ast_node_type::key_escaped }))
///             return std::nullopt;
///
///         auto key = from.current().visit_key([](const auto& k) { return std::string(k.value()); });
///         if (key == "a")
///         {
///             if (!from.next_token())
///                 return std::nullopt;
///
///             if (auto node = from.current_as<jsonv::ast_node::integer>())
///                 out.a = node->value();
///             else
///                 return std::nullopt;
///
///             // Step off the value and onto the next key, or onto the closing }.
///             if (!from.next_token())
///                 return std::nullopt;
///         }
///         else
///         {
///             // A key we do not care about -- skip its value, however large, and land on the next key.
///             if (!from.next_key())
///                 return std::nullopt;
///         }
///     }
///     return out;
/// }
/// \endcode
///
/// Note that \c next_key is only valid while sitting on a key or on the opening \c { -- it is the "skip this member"
/// step, not the loop's advance. Use \c next_token to move off a value you have just read.
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
    explicit reader(std::string_view source);
    explicit reader(std::string_view source, const parse_options& parse_options);
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
    JSONV_NODISCARD
    bool good() const;

    /// Get the current AST node this reader is pointing at.
    ///
    /// \throws std::logic_error if this instance is not \c good, or std::invalid_argument if it has been moved-from.
    JSONV_NODISCARD
    const ast_node& current() const;

    /// \{
    /// Check that the \c current AST node has the given \a type or is one of the expected \a types.
    ///
    /// \returns Nothing if the \c current node matches \a type or one of the given \a types; otherwise the
    ///          \c ast_node_type the \c current node actually has.
    /// \throws std::invalid_argument if \a types is empty.
    /// \throws std::logic_error if this instance is not \c good, or std::invalid_argument if it has been moved-from.
    ///
    /// \see ast_node::expect
    JSONV_NODISCARD
    std::expected<void, ast_node_type> expect(ast_node_type type) const;
    JSONV_NODISCARD
    std::expected<void, ast_node_type> expect(std::initializer_list<ast_node_type> types) const;
    /// \}

    /// Get the \c current AST node as a specific \c TAstNode subtype, calling \c expect beforehand.
    ///
    /// \returns The \c current node as a \c TAstNode; otherwise the \c ast_node_type the \c current node actually has.
    /// \throws std::logic_error if this instance is not \c good, or std::invalid_argument if it has been moved-from.
    template <typename TAstNode>
    JSONV_NODISCARD
    std::expected<TAstNode, ast_node_type> current_as() const
    {
        // Written as an explicit branch rather than `expect(...).transform(...)` on purpose. The monadic operations on
        // `std::expected` are a later addition than the type itself -- libstdc++ 12 and libc++ 16 have `expected` but
        // no `transform` -- so using one here would quietly raise the minimum toolchain by a whole release.
        if (auto matched = expect(TAstNode::type()); !matched)
            return std::unexpected(matched.error());
        else
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
    /// \throws std::logic_error if this instance is not \c good, or std::invalid_argument if it has been moved-from.
    JSONV_NODISCARD
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
    JSONV_NODISCARD
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
    /// The point of this over \ref next_token is abandoning a structure you are only part-way through. Once the one
    /// member you came for has been read, there is no reason to walk the rest of the object:
    ///
    /// \code
    /// /// Find the "a" member of an object and leave the rest of it unread.
    /// std::optional<std::int64_t> find_a(jsonv::reader& from)
    /// {
    ///     if (!from.expect(jsonv::ast_node_type::object_begin))
    ///         return std::nullopt;
    ///
    ///     if (!from.next_token())
    ///         return std::nullopt;
    ///
    ///     while (from.good() && from.current().type() != jsonv::ast_node_type::object_end)
    ///     {
    ///         if (!from.expect({ jsonv::ast_node_type::key_canonical, jsonv::ast_node_type::key_escaped }))
    ///             return std::nullopt;
    ///
    ///         auto key = from.current().visit_key([](const auto& k) { return std::string(k.value()); });
    ///         if (key != "a")
    ///         {
    ///             if (!from.next_key())
    ///                 return std::nullopt;
    ///
    ///             continue;
    ///         }
    ///
    ///         if (!from.next_token())
    ///             return std::nullopt;
    ///
    ///         auto node = from.current_as<jsonv::ast_node::integer>();
    ///
    ///         // Whatever is left of this object, we are done with it.
    ///         (void) from.next_structure();
    ///
    ///         if (node)
    ///             return node->value();
    ///         else
    ///             return std::nullopt;
    ///     }
    ///     return std::nullopt;
    /// }
    /// \endcode
    ///
    /// Note the call site: \c next_structure is used while sitting on a *value* inside the object, which is where it
    /// differs from \ref next_token. Called on the closing \c } itself it is merely \ref next_token, since there is no
    /// longer a structure to leave.
    ///
    /// \returns \c true if the reader is still \c good to read from \c current.
    JSONV_NODISCARD
    bool next_structure() noexcept;

    /// Go to one past the value this reader is on.
    ///
    /// Unlike \ref next_structure, which leaves the structure the reader is *inside*, this steps over the single value
    /// the reader is *on*. On a structure that means its matching close token; on anything else it is the same as
    /// \ref next_token.
    ///
    /// \code
    /// ^
    /// {
    ///   "a":   /* <- go to [ */
    ///     [    /* <- go to "b" */
    ///       1, /* <- go to 2 */
    ///       2,
    ///       3,
    ///     ],
    ///   "b":
    ///     {    /* <- go to "c" */
    ///     },
    ///   "c":
    ///     4    /* <- go to } */
    /// }
    /// $
    /// \endcode
    ///
    /// This is the primitive for ignoring a value you do not want. Note the difference from \ref next_structure at the
    /// `4` above: this goes to the `}`, while \ref next_structure leaves the enclosing object entirely.
    ///
    /// \returns \c true if the reader is still \c good to read from \c current.
    JSONV_NODISCARD
    bool next_value() noexcept;

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
    JSONV_NODISCARD
    bool next_key();

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
