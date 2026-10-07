/// \file jsonv/serialization/enum_adapter.hpp
///
/// Copyright (c) 2015-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/ast.hpp>
#include <jsonv/config.hpp>
#include <jsonv/functional.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>

#include <expected>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <string_view>
#include <utility>

#include "adapter_for.hpp"

namespace jsonv
{

namespace detail
{

/// Where a \c kind::string holding some text sorts against a \c value under \c value_less.
struct enum_text_order
{
    JSONV_NODISCARD
    static int compare(const value& a, std::string_view b)
    {
        return compare_text(a, b);
    }
};

/// Where a \c kind::string holding some text sorts against a \c value under \c value_less_icase.
struct enum_text_order_icase
{
    JSONV_NODISCARD
    static int compare(const value& a, std::string_view b)
    {
        return compare_text_icase(a, b);
    }
};

/// Orders the JSON side of an \c enum_adapter mapping by \c FValueComp, and places the text of a string among it by
/// \c TTextOrder -- which has to be where \c FValueComp would place a \c value holding that string, or a lookup could
/// miss what the table holds. That is what lets a string read from JSON text be looked up without building a \c value
/// to hold it.
template <typename FValueComp, typename TTextOrder>
struct enum_value_less
{
    using is_transparent = void;

    JSONV_NODISCARD
    bool operator()(const value& a, const value& b) const
    {
        return FValueComp()(a, b);
    }

    JSONV_NODISCARD
    bool operator()(const value& a, std::string_view b) const
    {
        return TTextOrder::compare(a, b) < 0;
    }

    JSONV_NODISCARD
    bool operator()(std::string_view a, const value& b) const
    {
        // Reversed rather than negated: a comparison may return INT_MIN, which has no negation.
        return TTextOrder::compare(b, a) > 0;
    }
};

/// How an \c enum_adapter orders its mapping, given the \c FValueComp it was asked for. An ordering the library does not
/// know can only be asked about a \c value, so it is used as it is, and a string read from JSON text is built into one
/// to be looked up. A transparent one is no exception: <tt>std::less&lt;&gt;</tt> cannot place a \c std::string_view
/// before a \c value.
template <typename FValueComp>
struct enum_value_order
{
    using type = FValueComp;

    static constexpr bool reads_text = false;
};

template <>
struct enum_value_order<std::less<value>>
{
    using type = enum_value_less<std::less<value>, enum_text_order>;

    static constexpr bool reads_text = true;
};

template <>
struct enum_value_order<value_less>
{
    using type = enum_value_less<value_less, enum_text_order>;

    static constexpr bool reads_text = true;
};

template <>
struct enum_value_order<value_less_icase>
{
    using type = enum_value_less<value_less_icase, enum_text_order_icase>;

    static constexpr bool reads_text = true;
};

}

/// \addtogroup Serialization
/// \{

/// An adapter for enumeration types. The most common use of this is to map \c enum values in C++ to string values in a
/// JSON representation (and vice versa).
///
/// Deserialization reads the \c reader directly. A value the mapping does not hold is refused with the reader still on
/// it, and the problem names the value and lists every JSON value the mapping accepts, in the mapping's order:
///
/// \code
/// Invalid value for ring: "bogus" (expected one of "earth", "fire", "heart", "useless", "water", "wind")
/// \endcode
///
/// \tparam TEnum The type to map. This is not restricted to C++ enumerations (types defined with the \c enum keyword),
///               but any type you wish to restrict to a subset of values.
/// \tparam FEnumComp <tt>bool (*)(TEnum, TEnum)</tt> -- a strict ordering for \c TEnum values.
/// \tparam FValueComp <tt>bool (*)(value, value)</tt> -- a strict ordering for \c value objects. By default, this is a
///                    case-sensitive comparison, but this can be replaced with anything you desire (for example, use
///                    \c value_less_icase to ignore case in deserializing from JSON). Under the library's own orderings
///                    -- <tt>std::less&lt;value&gt;</tt>, \c value_less and \c value_less_icase -- a string read from
///                    JSON text is looked up by its text, with no \c value built to hold it. Any other ordering can
///                    only be asked about a \c value, so one is built for every string deserialized from text.
///
/// \see enum_adapter_icase
template <typename TEnum,
          typename FEnumComp  = std::less<TEnum>,
          typename FValueComp = std::less<value>
         >
class enum_adapter :
        public adapter_for<TEnum>
{
public:
    /// Create an adapter with mapping values from the range <tt>[first, last)</tt>.
    ///
    /// \tparam TForwardIterator An iterator yielding the type <tt>std::pair&lt;TEnum, jsonv::value&gt;</tt>
    template <typename TForwardIterator>
    explicit enum_adapter(std::string enum_name, TForwardIterator first, TForwardIterator last) :
            _enum_name(std::move(enum_name))
    {
        for (auto iter = first; iter != last; ++iter)
        {
            _val_to_cpp.insert({ iter->second, iter->first });
            _cpp_to_val.insert(*iter);
        }
    }

    /// Create an adapter with the specified \a mapping values.
    ///
    /// \param enum_name A user-friendly name for this enumeration to be used in error messages.
    /// \param mapping A list of C++ types and values to use in \c to_json and \c deserialize. It is okay to have a C++
    ///                value with more than one JSON representation. In this case, the \e first JSON representation will
    ///                be used in \c to_json, but \e all JSON representations will be interpreted as the C++ value. It
    ///                is also okay to have the same JSON representation for multiple C++ values. In this case, the
    ///                \e first JSON representation provided for that value will be used in \c deserialize.
    ///
    /// For example:
    ///
    /// \code
    /// enum_adapter<ring>("ring",
    ///                    {
    ///                      { ring::fire,  "fire"    },
    ///                      { ring::wind,  "wind"    },
    ///                      { ring::earth, "earth"   },
    ///                      { ring::water, "water"   },
    ///                      { ring::heart, "heart"   }, // "heart" is preferred for to_json
    ///                      { ring::heart, "useless" }, // "useless" is interpreted as ring::heart in deserialize
    ///                    }
    ///                   );
    /// \endcode
    explicit enum_adapter(std::string enum_name, std::initializer_list<std::pair<TEnum, value>> mapping) :
            enum_adapter(std::move(enum_name), mapping.begin(), mapping.end())
    { }

protected:
    JSONV_NODISCARD
    virtual std::expected<TEnum, ast_node_type> create(deserialization_context& context, reader& from) const override
    {
        // A value-backed reader lends the value itself, which is looked up where it sits.
        if (auto lent = from.current_value())
            return settle(context, from, *lent);

        if constexpr (value_order::reads_text)
        {
            // An unescaped string is looked up by a view of the source, so nothing is built for it. One written with
            // escapes has no contiguous form to compare against until it is decoded.
            if (from.good() && from.current_type() == ast_node_type::string_canonical)
                return settle(context, from, from.current().as<ast_node::string_canonical>().value());

            if (from.good() && from.current_type() == ast_node_type::string_escaped)
            {
                const std::string decoded = from.current().as<ast_node::string_escaped>().value();
                return settle(context, from, std::string_view(decoded));
            }
        }

        // Anything else is built as a `value` without moving the reader -- a scalar where it sits, a structure through a
        // second cursor -- so a miss leaves this one on the value it is about.
        return settle(context, from, detail::peek_value(context, from));
    }

    virtual void serialize(const serialization_context&, const TEnum& from, writer& to) const override
    {
        using std::end;

        auto iter = _cpp_to_val.find(from);
        if (iter != end(_cpp_to_val))
            to.write(iter->second);
        else
            to.null();
    }

private:
    using value_order = detail::enum_value_order<FValueComp>;

    /// Look up \a key -- a \c value, or the text of a string -- and step \a from past the value it was read from.
    template <typename TKey>
    std::expected<TEnum, ast_node_type> settle(deserialization_context& context, reader& from, const TKey& key) const
    {
        auto iter = _val_to_cpp.find(key);
        if (iter == _val_to_cpp.end())
        {
            // The cursor is still on the value, which is where a problem about it belongs and what whoever recovers
            // from it expects to step over. Text is described as the string `value` it would have been, so the
            // message spells it as JSON.
            return context.problem(context.problem_path(from), describe_miss(value(key)));
        }

        // Copied while the cursor is still on the value, so a `TEnum` which refuses to be copied fails with the value in
        // front of it. Moving it out is the last thing which can fail, and by then the value is behind the cursor.
        TEnum out = iter->second;
        (void) from.next_value();
        try
        {
            return out;
        }
        catch (...)
        {
            context.note_value_consumed(from);
            throw;
        }
    }

    /// The message a miss on \a found is reported with: the value as JSON, then every one the mapping would accept.
    std::string describe_miss(const value& found) const
    {
        std::string message = "Invalid value for " + _enum_name + ": " + to_string(found);
        if (!_val_to_cpp.empty())
        {
            const char* separator = " (expected one of ";
            for (const auto& accepted : _val_to_cpp)
            {
                message += separator;
                message += to_string(accepted.first);
                separator = ", ";
            }
            message += ')';
        }
        return message;
    }

private:
    std::string                                        _enum_name;
    std::map<value, TEnum, typename value_order::type> _val_to_cpp;
    std::map<TEnum, value, FEnumComp>                  _cpp_to_val;
};

/// An adapter for enumeration types which ignores the case when deserializing from JSON.
///
/// \see enum_adapter
template <typename TEnum, typename FEnumComp = std::less<TEnum>>
using enum_adapter_icase = enum_adapter<TEnum, FEnumComp, value_less_icase>;

/// \}

}
