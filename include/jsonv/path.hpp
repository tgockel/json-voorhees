/// \file jsonv/path.hpp
/// Support for [JSONPath](http://goessner.net/articles/JsonPath/).
///
/// Copyright (c) 2014-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/detail/generic_container.hpp>
#include <string_view>

#include <concepts>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace jsonv
{

enum class path_element_kind : unsigned char
{
    array_index,
    object_key,
};

JSONV_PUBLIC std::ostream& operator<<(std::ostream&, const path_element_kind&);

JSONV_NODISCARD JSONV_PUBLIC std::string to_string(const path_element_kind&);

class JSONV_PUBLIC path_element
{
public:
    path_element(std::size_t idx);
    path_element(int         idx);
    path_element(std::string key);
    path_element(std::string_view key);
    path_element(const char* key);
    path_element(const path_element&);
    path_element& operator=(const path_element&);
    path_element(path_element&&) noexcept;
    path_element& operator=(path_element&&) noexcept;

    ~path_element() noexcept;

    JSONV_NODISCARD
    path_element_kind kind() const;

    JSONV_NODISCARD
    std::size_t index() const;

    JSONV_NODISCARD
    const std::string& key() const;

    JSONV_NODISCARD
    bool operator==(const path_element&) const;
    JSONV_NODISCARD
    bool operator!=(const path_element&) const;

private:
    union storage
    {
        std::size_t index;
        std::string key;

        storage(std::size_t   idx);
        storage(std::string&& key);
        ~storage() noexcept;
    };

private:
    path_element_kind _kind;
    storage           _data;
};

JSONV_PUBLIC std::ostream& operator<<(std::ostream&, const path_element&);

namespace detail
{

/// What \c to_string of a \c path_element returns, out of line.
JSONV_NODISCARD JSONV_PUBLIC std::string path_element_to_string(const path_element& elem);

}

/// The text \c operator<< writes for \a elem, which is how it appears in \c to_string of a \c path: <tt>[N]</tt> for an
/// array index, and <tt>.key</tt> or a JSON string in brackets for an object key.
///
/// This is a template only so that a \c path_element is the one thing it takes. A \c path_element converts from numbers
/// and strings, as a \c value does, so a plain function taking one made <tt>to_string(5)</tt> and
/// <tt>to_string("x")</tt> ambiguous with <tt>to_string(const value&)</tt> wherever this header was included. They mean
/// the \c value.
template <std::same_as<path_element> T>
JSONV_NODISCARD std::string to_string(const T& elem)
{
    return detail::path_element_to_string(elem);
}

/// Represents an exact path in some JSON structure.
class JSONV_PUBLIC path :
        public detail::generic_container<std::vector<path_element>>
{
public:
    /// Creates a new, empty path.
    path();

    /// Creates a path with the provided \a elements.
    path(storage_type elements);

    /// Create a \c path from a string definition. The syntax of this is ECMAScript's syntax for selecting elements, so
    /// <tt>path::create(".foo.bar[1]")</tt> is equivalent to <tt>path({ "foo", "bar", 1 })</tt>.
    ///
    /// \throws std::invalid_argument if the \a specification is not valid, including an array index too large for a
    ///                               \c std::size_t or a key with an unpaired UTF-16 surrogate escape.
    JSONV_NODISCARD
    static path create(std::string_view specification);

    path(const path&);
    path& operator=(const path&);
    path(path&&) noexcept;
    path& operator=(path&&) noexcept;
    ~path() noexcept;

    /// Return a new path with the given \a subpath appended to the back.
    JSONV_NODISCARD
    path  operator+(const path& subpath) const;
    path& operator+=(const path& subpath);

    /// Return a new path with the given \a elem appended to the back.
    JSONV_NODISCARD
    path  operator+(path_element elem) const;
    path& operator+=(path_element elem);
};

/// \{

/// Write \a val in the syntax \c path::create reads. An object key which is an identifier (matching
/// <tt>[a-zA-Z_$][a-zA-Z0-9_$]*</tt>) is written as <tt>.key</tt> and any other key as a JSON string in brackets, like
/// <tt>["a b"]</tt>. Well-formed UTF-8 in a key is written as it is. An array index is written as <tt>[N]</tt> whatever
/// the stream's locale, and the empty path as <tt>.</tt>.
///
/// So <tt>path::create(to_string(p)) == p</tt> for every path whose keys are well-formed UTF-8. A key which is not has
/// each ill-formed byte written as a <tt>\\u00NN</tt> escape, and that reads back as the codepoint U+00NN rather than
/// the byte.
JSONV_PUBLIC std::ostream& operator<<(std::ostream&, const path& val);

JSONV_NODISCARD JSONV_PUBLIC std::string to_string(const path& val);
/// \}

}
