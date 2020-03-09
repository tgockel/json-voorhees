/// \file jsonv/serialization/polymorphic_adapter.hpp
///
/// Copyright (c) 2017-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/serialization.hpp>

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
/// \tparam TPointer Some pointer-like type (likely \c unique_ptr or \c shared_ptr) you wish to extract values into. It
///                  must support \c operator*, an explicit conversion to \c bool, construction with a pointer to a
///                  subtype of what it contains and default construction.
///
template <typename TPointer>
class polymorphic_adapter :
        public adapter_for<TPointer>
{
public:
    using match_predicate = std::function<bool (const extraction_context&, const value&)>;

public:
    polymorphic_adapter() = default;

    /// Add a subtype which can be transformed into \c TPointer which will be called if the discriminator \a pred is
    /// matched.
    ///
    /// \see add_subtype_keyed
    template <typename T>
    void add_subtype(match_predicate pred)
    {
        _subtype_ctors.emplace_back(std::move(pred),
                                    [] (const extraction_context& context, const value& value)
                                    {
                                        return TPointer(new T(context.extract<T>(value)));
                                    }
                                   );
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

        match_predicate op = [key, expected_value] (const extraction_context&, const value& value)
                             {
                                 if (!value.is_object())
                                     return false;
                                 auto iter = value.find(key);
                                 return iter != value.end_object()
                                     && iter->second == expected_value;
                             };
        return add_subtype<T>(op);
    }

    /// \{
    /// When extracting a C++ value, should \c kind::null in JSON automatically become a default-constructed \c TPointer
    /// (which is usually the \c null representation)?
    void check_null_input(bool on)
    {
        _check_null_input = on;
    }

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

    bool check_null_output() const
    {
        return _check_null_output;
    }
    /// \}

protected:
    virtual TPointer create(const extraction_context& context, const value& from) const override
    {
        using std::begin;
        using std::end;

        if (_check_null_input && from.is_null())
            return TPointer();

        auto iter = std::find_if(begin(_subtype_ctors), end(_subtype_ctors),
                                 [&] (const std::pair<match_predicate, create_function>& pair)
                                 {
                                     return pair.first(context, from);
                                 }
                                );
        if (iter != end(_subtype_ctors))
            return iter->second(context, from);
        else
            throw extraction_error(context.path(),
                                   std::string("No discriminators matched JSON value: ") + to_string(from)
                                  );
    }

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
    using create_function = std::function<TPointer (const extraction_context&, const value&)>;

private:
    using serialization_action = std::tuple<std::string, value, keyed_subtype_action>;

    std::vector<std::pair<match_predicate, create_function>> _subtype_ctors;
    std::map<std::type_index, serialization_action>          _serialization_actions;
    bool                                                     _check_null_input  = false;
    bool                                                     _check_null_output = false;
};

/// \}

}
