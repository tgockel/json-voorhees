/// \file jsonv/parse_index.hpp
/// Parsed index of a JSON document.
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/ast.hpp>

#include <optional>

namespace jsonv
{

class extract_options;
class parse_options;

/// \ingroup Value
/// \{

/// Represents the index of a parsed AST. When combined with the original text, can be used to create a \c value. See
/// \c parse_index::parse to construct instances from JSON source text.
class JSONV_PUBLIC parse_index final
{
public:
    class iterator final
    {
    public:
        using value_type = ast_node;

    public:
        iterator() = default;

        iterator& operator++();
        iterator  operator++(int)
        {
            iterator temp(*this);
            ++*this;
            return temp;
        }

        /// Move to the node after the structure this iterator is on, however much is inside it.
        ///
        /// This is a constant-time operation. The index records where each structure ends as it parses the matching
        /// close token, so stepping over a subtree costs the same whether it holds one element or a million. Use it to
        /// ignore a value without paying to walk it.
        ///
        /// \code
        /// // `iter` is on the `[` of `"a": [ 1, 2, 3 ]`
        /// iter.skip_subtree();    // now on the key which follows the array
        /// \endcode
        ///
        /// If the structure was never closed -- only possible when \ref parse_index::success is \c false -- the
        /// error which truncated the document stands in as its end, so this moves to the end of the index.
        ///
        /// \throws std::invalid_argument if this iterator is not on a \c ast_node_type::document_start,
        ///  \c ast_node_type::object_begin or \c ast_node_type::array_begin.
        iterator& skip_subtree();

        JSONV_NODISCARD
        value_type operator*() const;

        // Note the lack of comparison between `_prefix` -- valid `iterator`s will always have the same `_prefix`
        JSONV_NODISCARD
        bool operator==(const iterator& other) const { return _iter == other._iter; }
        JSONV_NODISCARD
        bool operator!=(const iterator& other) const { return _iter != other._iter; }
        JSONV_NODISCARD
        bool operator< (const iterator& other) const { return _iter <  other._iter; }
        JSONV_NODISCARD
        bool operator<=(const iterator& other) const { return _iter <= other._iter; }
        JSONV_NODISCARD
        bool operator> (const iterator& other) const { return _iter >  other._iter; }
        JSONV_NODISCARD
        bool operator>=(const iterator& other) const { return _iter >= other._iter; }

    private:
        explicit iterator(const std::uintptr_t prefix, const std::uint64_t* iter) :
                _prefix(prefix),
                _iter(iter)
        { }

        friend class parse_index;

    private:
        std::uintptr_t       _prefix;
        const std::uint64_t* _iter;
    };

    using const_iterator = iterator;

public:
    /// Creates an empty not-an-AST instance.
    parse_index() noexcept = default;

    parse_index(parse_index&& src) noexcept :
            _impl(std::exchange(src._impl, nullptr))
    { }

    parse_index& operator=(parse_index&& src) noexcept
    {
        if (this == &src)
            return *this;

        using std::swap;
        swap(_impl, src._impl);
        src.reset();

        return *this;
    }

    ~parse_index() noexcept;

    /// \{
    /// Create an \c parse_index from the given \a src JSON.
    ///
    /// \param options
    ///     The options used to control parsing. If unspecified, these will be \c parse_options::create_default().
    /// \param initial_buffer_capacity
    ///     The initial capacity of the underlying buffer. By default (\c nullopt), this will size the buffer according
    ///     to the length of the \a src string.
    JSONV_NODISCARD
    static parse_index parse(std::string_view           src,
                             const parse_options&       options,
                             std::optional<std::size_t> initial_buffer_capacity
                            );
    JSONV_NODISCARD
    static parse_index parse(std::string_view src, std::optional<std::size_t> initial_buffer_capacity);
    JSONV_NODISCARD
    static parse_index parse(std::string_view src, const parse_options& options);
    JSONV_NODISCARD
    static parse_index parse(std::string_view src);
    /// \}

    /// Clear the contents of this instance.
    void reset();

    /// Check if this instance represents a valid AST. This will be \c false if the source JSON was not valid JSON text.
    /// This will also be \c false if this instance was default-constructed or moved-from.
    ///
    /// \note
    /// Even if this returns true, it is possible that conversion to a \c jsonv::value will throw an exception. For
    /// example, if the value of a number exceeds the range of an \c int64_t. This is because JSON does not specify an
    /// acceptable range for numbers, but the storage of \c jsonv::value does.
    JSONV_NODISCARD
    bool success() const noexcept;

    /// See \ref success.
    JSONV_NODISCARD
    explicit operator bool() const noexcept;

    /// Validate that the parse was a \c success.
    ///
    /// \throws parse_error if the parse was not successful. This will contain additional details about why the parse
    ///  failed.
    /// \throws std::invalid_argument if this instance was default-constructed or moved-from.
    void validate() const;

    JSONV_NODISCARD
    iterator begin() const;
    JSONV_NODISCARD
    iterator cbegin() const { return begin(); }

    JSONV_NODISCARD
    iterator end() const;

    JSONV_NODISCARD
    iterator cend() const { return end(); }

    /// \{
    /// \param options
    ///     The options used to control how values are extracted from this source. If unspecified, these will be
    ///     \c extract_options::create_default().
    JSONV_NODISCARD
    value extract_tree(const extract_options& options) const;
    JSONV_NODISCARD
    value extract_tree() const;
    /// \}

    /// \{
    /// Get a string representation of the AST.
    ///
    /// +--------------------+--------+
    /// | `ast_node_type`    | Output |
    /// +--------------------+--------+
    /// | `document_start`   | `^`    |
    /// | `document_end`     | `$`    |
    /// | `object_begin`     | `{`    |
    /// | `object_end`       | `}`    |
    /// | `array_begin`      | `[`    |
    /// | `array_end`        | `]`    |
    /// | `string_canonical` | `s`    |
    /// | `string_escaped`   | `S`    |
    /// | `key_canonical`    | `k`    |
    /// | `key_escaped`      | `K`    |
    /// | `literal_true`     | `t`    |
    /// | `literal_false`    | `f`    |
    /// | `literal_null`     | `n`    |
    /// | `integer`          | `i`    |
    /// | `decimal`          | `d`    |
    /// | `error`            | `!`    |
    /// +--------------------+--------+
    ///
    /// This exists primarily for debugging purposes.
    friend std::ostream& operator<<(std::ostream&, const parse_index&);
    friend std::string to_string(const parse_index&);
    /// \}

private:
    struct impl;

    explicit parse_index(impl*) noexcept;

private:
    impl* _impl = nullptr;
};

/// \}

}
