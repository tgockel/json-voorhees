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

namespace jsonv
{

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

        value_type operator*() const;

        // Note the lack of comparison between `_prefix` -- valid `iterator`s will always have the same `_prefix`
        bool operator==(const iterator& other) const { return _iter == other._iter; }
        bool operator!=(const iterator& other) const { return _iter != other._iter; }
        bool operator< (const iterator& other) const { return _iter <  other._iter; }
        bool operator<=(const iterator& other) const { return _iter <= other._iter; }
        bool operator> (const iterator& other) const { return _iter >  other._iter; }
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

    /// Create an \c parse_index from the given \a src JSON.
    ///
    /// \param initial_buffer_capacity The initial capacity of the underlying buffer. By default (\c nullopt), this will
    ///  size the buffer according to the length of the \a src string.
    static parse_index parse(string_view src, optional<std::size_t> initial_buffer_capacity = nullopt);

    /// Clear the contents of this instance.
    void reset();

    /// Check if this instance represents a valid AST. This will be \c false if the source JSON was not valid JSON text.
    /// This will also be \c false if this instance was default-constructed or moved-from.
    ///
    /// \note
    /// Even if this returns true, it is possible that conversion to a \c jsonv::value will throw an exception. For
    /// example, if the value of a number exceeds the range of an \c int64_t. This is because JSON does not specify an
    /// acceptable range for numbers, but the storage of \c jsonv::value does.
    bool success() const noexcept;

    /// Validate that the parse was a \c success.
    ///
    /// \throws parse_error if the parse was not successful. This will contain additional details about why the parse
    ///  failed.
    /// \throws std::invalid_argument if this instance was default-constructed or moved-from.
    void validate() const;

    iterator begin() const;
    iterator cbegin() const { return begin(); }

    iterator end() const;
    iterator cend() const { return end(); }

    value extract_tree(const parse_options& options) const;
    value extract_tree() const;

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
