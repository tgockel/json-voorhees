/// \file jsonv/parse.hpp
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <optional>
#include <string_view>
#include <jsonv/value.hpp>

#include <cstddef>
#include <stdexcept>

/// \def JSONV_PARSE_MAX_STRUCTURE_DEPTH
/// See \c jsonv::parse_options::k::max_structure_depth.
#ifndef JSONV_PARSE_MAX_STRUCTURE_DEPTH
#   define JSONV_PARSE_MAX_STRUCTURE_DEPTH 128
#endif

namespace jsonv
{

class deserialize_options;
class tokenizer;

/// An error encountered when parsing.
///
/// \see parse
class JSONV_PUBLIC parse_error :
        public std::runtime_error
{
public:
    /// \{

    /// Create an error with the given \a message and optional \a character location.
    explicit parse_error(const char* message, std::optional<std::size_t> character) noexcept;
    explicit parse_error(const char* message) noexcept;
    /// \}

    virtual ~parse_error() noexcept;

    /// Get the character location of the encountered error.
    JSONV_NODISCARD
    const std::optional<std::size_t>& character() const { return _character; }

private:
    std::optional<std::size_t> _character;
};

/// Get a string representation of a \c parse_error.
JSONV_PUBLIC std::ostream& operator<<(std::ostream& os, const parse_error& p);

/// Get a string representation of a \c parse_error.
JSONV_NODISCARD JSONV_PUBLIC std::string to_string(const parse_error& p);

/// Configuration for various parsing options. All parse functions should take in a \c parse_options as a paramter and
/// should respect your settings.
class JSONV_PUBLIC parse_options
{
public:
    using size_type = value::size_type;

    /** The encoding format for strings. **/
    enum class encoding
    {
        /// Default UTF-8 encoding scheme. Strings must be well-formed UTF-8 as Unicode defines it, which is the same
        /// as RFC 3629: every sequence is at most 4 bytes long and is the shortest encoding of its codepoint, and no
        /// codepoint is a UTF-16 surrogate (U+D800 through U+DFFF) or above U+10FFFF. A string which breaks any of
        /// these is a \c parse_error, as is an escape sequence naming an unpaired surrogate.
        ///
        /// \see http://www.unicode.org/versions/Unicode6.2.0/ch03.pdf#G7404
        utf8,
        /// Like \c utf8, but also reject unprintable ASCII characters in strings (see \c std::isprint). Where \c utf8
        /// accepts a raw \c tab or \c newline character inside a string, this mode rejects it. UTF-8 validation is
        /// identical in both modes -- the extra strictness is about ASCII control characters only.
        utf8_strict,
    };

    struct k final
    {
        /// The absolute maximum structure depth to parse. This sets the upper allowable limit
        static constexpr size_type max_structure_depth = size_type(JSONV_PARSE_MAX_STRUCTURE_DEPTH);
    };

public:
    /// Create an instance with the default options.
    parse_options();

    ~parse_options() noexcept; // NOLINT(performance-trivially-destructible): see #286

    /// Create a parser with the default options -- this is the same result as the default constructor, but might be
    /// helpful if you like to be more explicit.
    JSONV_NODISCARD
    static parse_options create_default();

    /// Create a strict parser. In general, these options are meant to fail on anything that is not a 100% valid JSON
    /// document. More specifically:
    ///
    /// \code
    /// string_encoding() == encoding::utf8_strict
    /// max_structure_depth() == 20
    /// require_document() == true
    /// comments() == false
    /// \endcode
    JSONV_NODISCARD
    static parse_options create_strict();

    /// \{

    /// The output encoding for multi-byte characters in strings. The default value is \c encoding::utf8.
    JSONV_NODISCARD
    encoding       string_encoding() const { return _string_encoding; }
    parse_options& string_encoding(encoding);
    /// \}

    /// \{

    /// The maximum allowed nesting depth of any structure in the JSON document. The JSON specification technically
    /// limits the depth to 20, but very few implementations actually conform to this, so it is fairly dangerous to set
    /// this value. By default, the value is \c nullopt, which means implementations should limit structure depth to
    /// \c k::max_structure_depth. Setting \a depth to a value above \c k::max_structure_depth is will cause
    /// \c std::invalid_argument to be thrown from \c parse functions.
    JSONV_NODISCARD
    std::optional<size_type> max_structure_depth() const { return _max_struct_depth; }
    parse_options&           max_structure_depth(std::optional<size_type> depth);
    /// \}

    /// \{

    /// If set to true, the result of a parse is required to have \c kind of \c kind::object or \c kind::array. By
    /// default, this is turned off, which will allow \c parse to return values with \c kind::string or
    /// \c kind::integer.
    JSONV_NODISCARD
    bool           require_document() const { return _require_document; }
    parse_options& require_document(bool);
    /// \}

    /// \{

    /// Are JSON comments allowed? While there is no official syntax for JSON comments, this uses the de-facto standard
    /// of ECMAScript-style block comments: `/* comment */`. If this is enabled, comments are treated exactly like
    /// whitespace. This is off by default, since RFC 8259 has no comments and a conforming parser rejects them; turn
    /// it on for configuration files and other input written in that dialect.
    JSONV_NODISCARD
    bool           comments() const { return _comments; }
    parse_options& comments(bool);
    /// \}

private:
    // For the purposes of ABI compliance, most modifications to the variables in this class should bump the minor
    // version number.
    encoding            _string_encoding  = encoding::utf8;
    std::optional<size_type> _max_struct_depth = std::nullopt;
    bool                _require_document = false;
    bool                _comments         = false;
};

/// \{

/// Construct a JSON value from the given \a input.
///
/// \code
/// jsonv::value out = jsonv::parse(R"( { "a": 1, "b": [ 2, 3, 4 ] } )");
/// \endcode
///
/// \a parse_options controls parsing -- the indexing of source text into an AST -- and is \c
/// parse_options::create_default() where not given. \a deserialize_options controls extraction -- transforming the AST
/// into a \c jsonv::value -- and is \c deserialize_options::create_default() where not given.
///
/// An integer literal beyond the range of 64 bits is read as the nearest \c double, so it is a \c kind::decimal. One
/// from 2^63 through 2^64-1 keeps its bits and reads back as a negative \c kind::integer (see
/// \c ast_node::integer::value).
///
/// \throws parse_error if the source text is invalid JSON. This is thrown for errors like unterminated strings, arrays,
///  or stray literals. Errors of this category are described as an offset into \a input.
/// \throws deserialization_error if the AST can not be transformed into a \c jsonv::value. This is thrown for errors
///                               like an object with duplicate keys (note that \c replace, the default \c
///                               deserialize_options::on_duplicate_key, does not throw for this case).
/// \throws std::invalid_argument if a number literal has no finite \c double to round to, such as \c 1e400.
JSONV_NODISCARD JSONV_PUBLIC
value parse(std::string_view            input,
            const parse_options&   parse_options,
            const deserialize_options& deserialize_options
           );

JSONV_NODISCARD JSONV_PUBLIC value parse(std::string_view input);
JSONV_NODISCARD JSONV_PUBLIC value parse(std::string_view input, const parse_options& parse_options);
JSONV_NODISCARD JSONV_PUBLIC value parse(std::string_view input, const deserialize_options& deserialize_options);
/// \}

/// \{

/// Reads a JSON value from the \a input stream. For example, to parse JSON from a file:
///
/// \code
/// std::ifstream file("file.json");
/// jsonv::value out = parse(file);
/// \endcode
///
/// \a parse_options controls parsing -- the indexing of source text into an AST -- and is \c
/// parse_options::create_default() where not given. \a deserialize_options controls extraction -- transforming the AST
/// into a \c jsonv::value -- and is \c deserialize_options::create_default() where not given.
///
/// An integer literal beyond the range of 64 bits is read as the nearest \c double, so it is a \c kind::decimal. One
/// from 2^63 through 2^64-1 keeps its bits and reads back as a negative \c kind::integer (see
/// \c ast_node::integer::value).
///
/// \throws parse_error if the source text is invalid JSON. This is thrown for errors like unterminated strings, arrays,
///  or stray literals. Errors of this category are described as an offset into \a input.
/// \throws deserialization_error if the AST can not be transformed into a \c jsonv::value. This is thrown for errors
///                               like an object with duplicate keys (note that \c replace, the default \c
///                               deserialize_options::on_duplicate_key, does not throw for this case).
/// \throws std::invalid_argument if a number literal has no finite \c double to round to, such as \c 1e400.
JSONV_NODISCARD JSONV_PUBLIC
value parse(std::istream&          input,
            const parse_options&   parse_options,
            const deserialize_options& deserialize_options
           );

JSONV_NODISCARD JSONV_PUBLIC value parse(std::istream& input);
JSONV_NODISCARD JSONV_PUBLIC value parse(std::istream& input, const parse_options& parse_options);
JSONV_NODISCARD JSONV_PUBLIC value parse(std::istream& input, const deserialize_options& deserialize_options);
/// \}

/// \{

/// Read a JSON value from the string bound by `[begin, end)`.
JSONV_NODISCARD JSONV_PUBLIC
value parse(const char*            begin,
            const char*                end,
            const parse_options&       parse_options,
            const deserialize_options& deserialize_options
           );
JSONV_NODISCARD JSONV_PUBLIC value parse(const char* begin, const char* end, const parse_options& parse_options);
JSONV_NODISCARD JSONV_PUBLIC value parse(const char*                begin,
                                         const char*                end,
                                         const deserialize_options& deserialize_options
                                        );
JSONV_NODISCARD JSONV_PUBLIC value parse(const char* begin, const char* end);
/// \}

}
