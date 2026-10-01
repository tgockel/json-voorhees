/// \file jsonv/serialization/polymorphic_adapter.hpp
///
/// Copyright (c) 2017-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/kind.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/serialization.hpp>

#include <expected>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "adapter_for.hpp"

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// What to do when serializing a keyed subtype of a \c polymorphic_adapter. See
/// \c polymorphic_adapter::add_subtype_keyed.
enum class keyed_subtype_action : unsigned char
{
    /// Don't do any checking or insertion of the expected key/value pair.
    none,
    /// Ensure the correct key/value pair was inserted by serialization. Throws \c std::runtime_error if it wasn't.
    check,
    /// Insert the correct key/value pair as part of serialization. Throws \c std::runtime_error if the key is already
    /// present.
    insert
};

/// An adapter which can create polymorphic types. This allows you to parse JSON directly into a type heirarchy without
/// some middle layer.
///
/// @code
/// [
///   {
///     "type": "worker",
///     "name": "Adam"
///   },
///   {
///     "type": "manager",
///     "name": "Bob",
///     "head": "Development"
///   }
/// ]
/// @endcode
///
/// Choosing a subtype means looking at the value before extracting it, which a \c reader -- a forward cursor -- cannot
/// do by itself. Each discriminator is shown as little of the value as answers it, read without moving the reader, and
/// the subtype it picks is then extracted from the reader directly:
///
///  - From a \c value, every discriminator is shown that value. Nothing is copied.
///  - From JSON text, one registered with \c add_subtype_keyed is shown an object holding only the discriminating
///    members, found by stepping over every other member whole. One registered with \c add_subtype can ask anything
///    of the value, so the first time one of those has to be asked the subtree is materialised for it. Registering
///    the keyed subtypes first keeps a matching document from ever being materialised.
///
/// \tparam TPointer Some pointer-like type (likely \c unique_ptr or \c shared_ptr) you wish to extract values into. It
///                  must support \c operator*, an explicit conversion to \c bool, construction with a pointer to a
///                  subtype of what it contains and default construction.
///
template <typename TPointer>
class polymorphic_adapter :
        public adapter_for<TPointer>
{
public:
    using match_predicate = std::function<bool (extraction_context&, const value&)>;

public:
    polymorphic_adapter() = default;

    /// Add a subtype which can be transformed into \c TPointer which will be called if the discriminator \a pred is
    /// matched.
    ///
    /// \see add_subtype_keyed
    template <typename T>
    void add_subtype(match_predicate pred)
    {
        emplace_subtype<T>(std::move(pred), false);
    }

    /// Add a subtype which can be transformed into \c TPointer which will be called if given a JSON \c value with
    /// \c kind::object which has a member with \a key and the provided \a expected_value.
    ///
    /// \see add_subtype
    template <typename T>
    void add_subtype_keyed(std::string key,
                           value expected_value,
                           keyed_subtype_action action = keyed_subtype_action::none)
    {
        std::type_index tidx = std::type_index(typeid(T));
        if (!_serialization_actions.emplace(tidx, std::make_tuple(key, expected_value, action)).second)
            throw duplicate_type_error("polymorphic_adapter subtype", std::type_index(typeid(T)));

        // Recorded before the subtype, so a failure to record it cannot leave a keyed subtype whose key is never read.
        _discriminator_keys.insert(key);

        match_predicate op = [key = std::move(key), expected_value = std::move(expected_value)]
                             (extraction_context&, const value& value)
                             {
                                 if (!value.is_object())
                                     return false;
                                 auto iter = value.find(key);
                                 return iter != value.end_object()
                                     && iter->second == expected_value;
                             };
        emplace_subtype<T>(std::move(op), true);
    }

    /// \{
    /// When extracting a C++ value, should \c kind::null in JSON automatically become a default-constructed \c TPointer
    /// (which is usually the \c null representation)?
    void check_null_input(bool on)
    {
        _check_null_input = on;
    }

    JSONV_NODISCARD
    bool check_null_input() const
    {
        return _check_null_input;
    }
    /// }

    /// \{
    /// When converting with \c to_json, should a \c null input translate into a \c kind::null?
    void check_null_output(bool on)
    {
        _check_null_output = on;
    }

    JSONV_NODISCARD
    bool check_null_output() const
    {
        return _check_null_output;
    }
    /// \}

protected:
    JSONV_NODISCARD
    virtual std::expected<TPointer, ast_node_type> create(extraction_context& context, reader& from) const override
    {
        // A value-backed reader renders a non-finite `kind::decimal` as `literal_null`, so where there is a `value` to
        // ask, its `kind` decides -- the same rule `optional_adapter` follows for the same reason.
        const value* lent = from.current_value();
        if (_check_null_input && (lent ? lent->kind() == jsonv::kind::null
                                       : from.current_type() == ast_node_type::literal_null
                                 )
           )
        {
            // The cursor steps before the pointer is built, so a `TPointer` which refuses to default-construct fails
            // with the value behind it.
            try
            {
                (void) from.next_token();
                return TPointer();
            }
            catch (...)
            {
                context.note_value_consumed(from);
                throw;
            }
        }

        // Each is read at most once, and only if some discriminator needs it. Neither moves `from`. Both settle a
        // repeated key the way the subtype will when it reads the document, or the subtype chosen and the one built
        // could disagree about what the discriminator said.
        std::optional<value> members;
        std::optional<value> whole;
        auto subject = [&] (const subtype& sub) -> const value&
                       {
                           if (lent)
                               return *lent;

                           // A keyed discriminator reads one member, which the whole subtree has just as well as the
                           // projection does -- so once the whole has been paid for, there is no reason to scan again.
                           if (sub.keyed && !whole)
                           {
                               if (!members)
                                   members.emplace(detail::peek_members(context, from, _discriminator_keys));
                               return *members;
                           }

                           if (!whole)
                               whole.emplace(detail::peek_value(context, from));
                           return *whole;
                       };

        const subtype* chosen = nullptr;
        {
            // From text, a discriminator is shown a copy which dies with this call, so a view of anything in it would
            // dangle. Saying so is what makes extracting one refuse, as it did when the bridge built that copy. From a
            // `value` it is shown the caller's own tree, which a view may name.
            std::optional<detail::temporary_source_scope> temporary;
            if (!lent)
                temporary.emplace(context);

            for (const auto& sub : _subtypes)
            {
                if (sub.predicate(context, subject(sub)))
                {
                    chosen = &sub;
                    break;
                }
            }
        }

        // Out of that scope, the chosen subtype reads the reader itself: a view it holds names the source rather than
        // a temporary, and a position it takes from the reader is in the document rather than in a copy of it.
        if (chosen)
            return chosen->create(context, from);

        // The cursor is still on the value, which is where a problem about it belongs and what whoever recovers from it
        // expects to step over.
        std::string message = "No discriminators matched JSON value";
        try
        {
            const value& unmatched = lent  ? *lent
                                   : whole ? *whole
                                   :         whole.emplace(detail::peek_value(context, from));
            message += ": " + to_string(unmatched);
        }
        catch (...) // NOLINT(bugprone-empty-catch): describing the failure must not replace it
        { }
        return context.problem(context.problem_path(from), std::move(message));
    }

    JSONV_NODISCARD
    virtual value to_json(const serialization_context& context, const TPointer& from) const override
    {
        if (_check_null_output && !from)
            return null;

        value serialized = context.to_json(typeid(*from), static_cast<const void*>(&*from));

        auto action_iter = _serialization_actions.find(std::type_index(typeid(*from)));
        if (action_iter != _serialization_actions.end())
        {
            auto errmsg = [&]()
                          {
                              return " polymorphic_adapter<" + demangle(typeid(TPointer).name()) + ">"
                                     "subtype(" + demangle(typeid(*from).name()) + ")";
                          };

            const std::string&          key    = std::get<0>(action_iter->second);
            const value&                val    = std::get<1>(action_iter->second);
            const keyed_subtype_action& action = std::get<2>(action_iter->second);

            switch (action)
            {
            case keyed_subtype_action::none:
                break;
            case keyed_subtype_action::check:
                if (!serialized.is_object())
                    throw std::runtime_error("Expected keyed subtype to serialize as an object." + errmsg());
                if (!serialized.count(key))
                    throw std::runtime_error("Expected subtype key not found." + errmsg());
                if (serialized.at(key) != val)
                    throw std::runtime_error("Expected subtype key is not the expected value." + errmsg());
                break;
            case keyed_subtype_action::insert:
                if (!serialized.is_object())
                    throw std::runtime_error("Expected keyed subtype to serialize as an object." + errmsg());
                if (serialized.count(key))
                    throw std::runtime_error("Subtype key already present when trying to insert." + errmsg());
                serialized[key] = val;
                break;
            default:
                throw std::runtime_error("Unknown keyed_subtype_action.");
            }
        }

        return serialized;
    }

private:
    using create_function = std::function<std::expected<TPointer, ast_node_type> (extraction_context&, reader&)>;

    struct subtype
    {
        match_predicate predicate;
        /// Does \ref predicate read nothing but a member named in \ref _discriminator_keys? If so it can be shown
        /// \c detail::peek_members in place of the whole value.
        bool            keyed;
        create_function create;
    };

    template <typename T>
    void emplace_subtype(match_predicate pred, bool keyed)
    {
        _subtypes.push_back(subtype{ std::move(pred),
                                     keyed,
                                     [] (extraction_context& context, reader& from)
                                             -> std::expected<TPointer, ast_node_type>
                                     {
                                         // A failure `extract` reports is returned, not thrown. What can throw is
                                         // everything after the cursor has stepped past the value: `extract` moving
                                         // the `T` it built out to here, the allocation, and the move into it -- all
                                         // of which fail with that value behind the cursor.
                                         try
                                         {
                                             auto extracted = context.extract<T>(from);
                                             if (!extracted)
                                                 return std::unexpected(extracted.error());

                                             return TPointer(new T(*std::move(extracted)));
                                         }
                                         catch (...)
                                         {
                                             context.note_value_consumed(from);
                                             throw;
                                         }
                                     }
                                   }
                           );
    }

private:
    using serialization_action = std::tuple<std::string, value, keyed_subtype_action>;

    std::vector<subtype>                            _subtypes;
    std::set<std::string, std::less<>>              _discriminator_keys;
    std::map<std::type_index, serialization_action> _serialization_actions;
    bool                                            _check_null_input  = false;
    bool                                            _check_null_output = false;
};

/// \}

}
