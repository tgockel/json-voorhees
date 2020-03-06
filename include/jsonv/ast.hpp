/// \file jsonv/ast.hpp
/// Utilities for directly dealing with a JSON AST. For most cases, it is more convenient to use \c jsonv::value.
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
#include <jsonv/kind.hpp>
#include <jsonv/optional.hpp>
#include <jsonv/string_view.hpp>

#include <cstdint>
#include <iosfwd>
#include <utility>
#include <variant>

namespace jsonv
{

class value;

namespace detail
{

inline string_view string_from_token(string_view token, std::false_type is_escaped JSONV_UNUSED)
{
    return string_view(token.data() + 1, token.size() - 2U);
}

std::string string_from_token(string_view token, std::true_type is_escaped);

}

/// \ingroup Value
/// \{

/// Marker type for an encountered token type.
///
/// - \c document_end
///   The end of a document.
/// - \c document_start
///   The beginning of a document.
/// - \c object_begin
///   The beginning of an \c kind::object (`{`).
/// - \c object_end
///   The end of an \c kind::object (`}`).
/// - \c array_begin
///   The beginning of an \c kind::array (`[`).
/// - \c array_end
///   The end of an \c kind::array (`]`).
/// - \c string_canonical
///   A \c kind::string whose JSON-encoded format matches the canonical UTF-8 representation. There is no need to
///   translate JSON escape sequences to extract a \c std::string value, so accessing the raw text is safe.
/// - \c string_escaped
///   A \c kind::string whose JSON-encoded format contains escape sequences, so it must be translated to extract a
///   \c std::string value.
/// - \c key_canonical
///   The \c ast_node_type::string_canonical key of an \c kind::object.
/// - \c key_escaped
///   The \c ast_node_type::string_escaped key of an \c kind::object.
/// - \c literal_true
///   The \c kind::boolean literal \c true.
/// - \c literal_false
///   The \c kind::boolean literal \c false.
/// - \c literal_null
///   The \c kind::null literal \c null.
/// - \c integer
///   An \c kind::integer value. No decimals or exponent symbols were encountered during parsing. Note that integers are
///   \e not bounds-checked by the AST -- values outside of \c std::int64_t are still \c integer values.
/// - \c decimal
///   A \c kind::decimal value.
/// - \c error
///   An AST parsing error.
///
/// \see parse_index
/// \see ast_node
enum class ast_node_type : std::uint8_t
{
    document_end     = 0,
    document_start   = 1,
    object_begin     = 2,
    object_end       = 3,
    array_begin      = 4,
    array_end        = 5,
    string_canonical = 6,
    string_escaped   = 7,
    key_canonical    = 8,
    key_escaped      = 9,
    literal_true     = 10,
    literal_false    = 11,
    literal_null     = 12,
    integer          = 13,
    decimal          = 14,
    error            = 15,
};

/// \{
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
JSONV_PUBLIC std::ostream& operator<<(std::ostream&, const ast_node_type& type);
JSONV_PUBLIC std::string to_string(const ast_node_type& type);
/// \}

/// Error code encountered while building the AST.
enum class ast_error : std::uint64_t
{
    none = 0,
    expected_document,
    expected_string,
    expected_key_delimiter,
    unexpected_token,
    unexpected_comma,
    eof,
    expected_eof,
    depth_exceeded,
    extra_close,
    mismatched_close,
    close_after_comma,
    invalid_literal,
    invalid_number,
    invalid_string,
    invalid_comment,
    internal,
};

/// \{
/// Get a description of the error \a code.
JSONV_PUBLIC std::ostream& operator<<(std::ostream&, const ast_error& code);
JSONV_PUBLIC std::string to_string(const ast_error& code);
/// \}

/// Represents an entry in a JSON AST.
///
/// \see parse_index
class ast_node final
{
public:
    template <typename TSelf, ast_node_type KIndexToken>
    class base
    {
    public:
        /// Get the \c ast_node_type type.
        constexpr ast_node_type type() const
        {
            return KIndexToken;
        }

        /// \see ast_node::token_raw
        string_view token_raw() const
        {
            return string_view(_token_begin, static_cast<const TSelf&>(*this).token_size());
        }

        /// Allow implicit conversion to the more generic \c ast_node.
        operator ast_node() const;

    protected:
        explicit constexpr base(const char* token_begin) :
                _token_begin(token_begin)
        { }

    private:
        const char* _token_begin;
    };

    template <typename TSelf, ast_node_type KIndexToken, std::size_t KTokenSize>
    class basic_fixed_size_token : public base<TSelf, KIndexToken>
    {
    public:
        explicit constexpr basic_fixed_size_token(const char* token_begin) :
                base<TSelf, KIndexToken>(token_begin)
        { }

        constexpr std::size_t token_size() const
        {
            return KTokenSize;
        }
    };

    template <typename TSelf, ast_node_type KIndexToken>
    class basic_dynamic_size_token : public base<TSelf, KIndexToken>
    {
    public:
        explicit constexpr basic_dynamic_size_token(const char* token_begin, std::size_t token_size) :
                base<TSelf, KIndexToken>(token_begin),
                _token_size(token_size)
        { }

        constexpr std::size_t token_size() const
        {
            return _token_size;
        }

    private:
        std::size_t _token_size;
    };

    /// The start of a document.
    class document_start final : public basic_fixed_size_token<document_start, ast_node_type::document_start, 0U>
    {
    public:
        using basic_fixed_size_token<document_start, ast_node_type::document_start, 0U>::basic_fixed_size_token;
    };

    /// The end of a document.
    class document_end final : public basic_fixed_size_token<document_end, ast_node_type::document_end, 0U>
    {
    public:
        using basic_fixed_size_token<document_end, ast_node_type::document_end, 0U>::basic_fixed_size_token;
    };

    /// The beginning of an \c kind::object (`{`).
    class object_begin final : public basic_fixed_size_token<object_begin, ast_node_type::object_begin, 1U>
    {
    public:
        explicit constexpr object_begin(const char* token_begin, std::size_t element_count) :
                basic_fixed_size_token<object_begin, ast_node_type::object_begin, 1U>(token_begin),
                _element_count(element_count)
        { }

        /// Get the number of elements in the object this token starts. This is useful for reserving memory.
        constexpr std::size_t element_count() const
        {
            return _element_count;
        }

    private:
        std::size_t _element_count;
    };

    /// The end of an \c kind::object (`}`).
    class object_end final : public basic_fixed_size_token<object_end, ast_node_type::object_end, 1U>
    {
    public:
        using basic_fixed_size_token<object_end, ast_node_type::object_end, 1U>::basic_fixed_size_token;
    };

    /// The beginning of an \c kind::array (`[`).
    class array_begin final : public basic_fixed_size_token<array_begin, ast_node_type::array_begin, 1U>
    {
    public:
        explicit constexpr array_begin(const char* token_begin, std::size_t element_count) :
                basic_fixed_size_token<array_begin, ast_node_type::array_begin, 1U>(token_begin),
                _element_count(element_count)
        { }

        /// Get the number of elements in the array this token starts. This is useful for reserving memory.
        constexpr std::size_t element_count() const
        {
            return _element_count;
        }

    private:
        std::size_t _element_count;
    };

    /// The end of an \c kind::array (`]`).
    class array_end final : public basic_fixed_size_token<array_end, ast_node_type::array_end, 1U>
    {
    public:
        using basic_fixed_size_token<array_end, ast_node_type::array_end, 1U>::basic_fixed_size_token;
    };

    template <typename TSelf, ast_node_type KIndexToken, bool KEscaped>
    class basic_string_token : public basic_dynamic_size_token<TSelf, KIndexToken>
    {
    public:
        /// The return type of \c value is based on if the string is \c canonical or \c escaped. Strings in canonical
        /// representation can be returned directly from the text through a \c string_view.
        using value_type = std::conditional_t<KEscaped, std::string, string_view>;

    public:
        explicit constexpr basic_string_token(const char* token_begin, std::size_t token_size) :
                basic_dynamic_size_token<TSelf, KIndexToken>(token_begin, token_size),
                _token_size(token_size)
        { }

        /// Was the source JSON for this string encoded in the canonical UTF-8 representation? If this is \c true, there
        /// is no need to translate JSON escape sequences to extract a \c std::string value. This is the opposite of
        /// \c escaped.
        constexpr bool canonical() const noexcept
        {
            return !KEscaped;
        }

        /// Did the source JSON for this string contain escape sequences? If this is \c true, JSON escape sequences must
        /// be translated into their canonical UTF-8 representation on extraction. This is the opposite of \c canonical.
        constexpr bool escaped() const noexcept
        {
            return KEscaped;
        }

        constexpr std::size_t token_size() const
        {
            return _token_size;
        }

        value_type value() const
        {
            return detail::string_from_token(this->token_raw(), std::integral_constant<bool, KEscaped>());
        }

    private:
        std::size_t _token_size;
    };

    class string_canonical final : public basic_string_token<string_canonical, ast_node_type::string_canonical, false>
    {
    public:
        using basic_string_token<string_canonical, ast_node_type::string_canonical, false>::basic_string_token;
    };

    class string_escaped final : public basic_string_token<string_escaped, ast_node_type::string_escaped, true>
    {
    public:
        using basic_string_token<string_escaped, ast_node_type::string_escaped, true>::basic_string_token;
    };

    class key_canonical final : public basic_string_token<key_canonical, ast_node_type::key_canonical, false>
    {
    public:
        using basic_string_token<key_canonical, ast_node_type::key_canonical, false>::basic_string_token;
    };

    class key_escaped final : public basic_string_token<key_escaped, ast_node_type::key_escaped, true>
    {
    public:
        using basic_string_token<key_escaped, ast_node_type::key_escaped, true>::basic_string_token;
    };

    class literal_true final : public basic_fixed_size_token<literal_true, ast_node_type::literal_true, 4U>
    {
    public:
        using basic_fixed_size_token<literal_true, ast_node_type::literal_true, 4U>::basic_fixed_size_token;

        bool value() const noexcept
        {
            return true;
        }
    };

    class literal_false final : public basic_fixed_size_token<literal_false, ast_node_type::literal_false, 5U>
    {
    public:
        using basic_fixed_size_token<literal_false, ast_node_type::literal_false, 5U>::basic_fixed_size_token;

        bool value() const noexcept
        {
            return false;
        }
    };

    class literal_null final : public basic_fixed_size_token<literal_null, ast_node_type::literal_null, 4U>
    {
    public:
        using basic_fixed_size_token<literal_null, ast_node_type::literal_null, 4U>::basic_fixed_size_token;

        jsonv::value value() const;
    };

    class integer final : public basic_dynamic_size_token<integer, ast_node_type::integer>
    {
    public:
        using basic_dynamic_size_token<integer, ast_node_type::integer>::basic_dynamic_size_token;

        std::int64_t value() const;
    };

    class decimal final : public basic_dynamic_size_token<decimal, ast_node_type::decimal>
    {
    public:
        using basic_dynamic_size_token<decimal, ast_node_type::decimal>::basic_dynamic_size_token;

        double value() const;
    };

    class error final : public basic_dynamic_size_token<error, ast_node_type::error>
    {
    public:
        explicit constexpr error(const char* token_begin, std::size_t token_size, ast_error error_code) :
                basic_dynamic_size_token<error, ast_node_type::error>(token_begin, token_size),
                _error_code(error_code)
        { }

        constexpr ast_error error_code() const
        {
            return _error_code;
        }

    private:
        ast_error   _error_code;
    };

    using storage_type = std::variant<document_end,
                                      document_start,
                                      object_begin,
                                      object_end,
                                      array_begin,
                                      array_end,
                                      string_canonical,
                                      string_escaped,
                                      key_canonical,
                                      key_escaped,
                                      literal_true,
                                      literal_false,
                                      literal_null,
                                      integer,
                                      decimal,
                                      error>;

public:
    ast_node(const storage_type& value) :
            _impl(value)
    { }

    template <typename T, typename... TArgs>
    explicit ast_node(std::in_place_type_t<T> type, TArgs&&... args) :
            _impl(type, std::forward<TArgs>(args)...)
    { }

    /// Get the \c std::variant that backs this type.
    ///
    /// \see visit
    const storage_type& storage() const
    {
        return _impl;
    }

    /// Convenience function for calling \c std::visit on the underlying \c storage of this node.
    template <typename FVisitor>
    auto visit(FVisitor&& visitor) const
    {
        return std::visit(std::forward<FVisitor>(visitor), _impl);
    }

    /// Get the \c ast_node_type that tells the underlying type of this instance.
    ast_node_type type() const
    {
        return visit([](const auto& x) { return x.type(); });
    }

    /// Get a view of the raw token. For example, \c "true", \c "{", or \c "1234". Note that this includes the complete
    /// source, so string types such as \c ast_node_type::string_canonical include the opening and closing quotations.
    string_view token_raw() const
    {
        return visit([](const auto& x) { return x.token_raw(); });
    }

    /// Get the underlying data of this node as \c T.
    ///
    /// \throws std::bad_variant_access if the requested \c T is different from the \c type of this instance.
    template <typename T>
    const T& as() const
    {
        return std::get<T>(_impl);
    }

private:
    storage_type _impl;
};

template <typename TSelf, ast_node_type KIndexToken>
ast_node::base<TSelf, KIndexToken>::operator ast_node() const
{
    return ast_node(ast_node::storage_type(static_cast<const TSelf&>(*this)));
}

/// \}

}
