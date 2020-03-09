/// \file jsonv/serialization/enum_adapter.hpp
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/functional.hpp>
#include <jsonv/serialization.hpp>

#include "adapter_for.hpp"

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// An adapter for enumeration types. The most common use of this is to map \c enum values in C++ to string values in a
/// JSON representation (and vice versa).
///
/// \tparam TEnum The type to map. This is not restricted to C++ enumerations (types defined with the \c enum keyword),
///               but any type you wish to restrict to a subset of values.
/// \tparam FEnumComp <tt>bool (*)(TEnum, TEnum)</tt> -- a strict ordering for \c TEnum values.
/// \tparam FValueComp <tt>bool (*)(value, value)</tt> -- a strict ordering for \c value objects. By default, this is a
///                    case-sensitive comparison, but this can be replaced with anything you desire (for example, use
///                    \c value_less_icase to ignore case in extracting from JSON).
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
    /// \tparam TForwardIterator An iterator yielding the type <tt>std::pair<jsonv::value, TEnum>></tt>
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
    /// \param mapping A list of C++ types and values to use in \c to_json and \c extract. It is okay to have a C++
    ///                value with more than one JSON representation. In this case, the \e first JSON representation will
    ///                be used in \c to_json, but \e all JSON representations will be interpreted as the C++ value. It
    ///                is also okay to have the same JSON representation for multiple C++ values. In this case, the
    ///                \e first JSON representation provided for that value will be used in \c extract.
    ///
    /// \example "Serialization: Enum Adapter"
    /// \code
    /// enum_adapter<ring>("ring",
    ///                    {
    ///                      { ring::fire,  "fire"    },
    ///                      { ring::wind,  "wind"    },
    ///                      { ring::earth, "earth"   },
    ///                      { ring::water, "water"   },
    ///                      { ring::heart, "heart"   }, // "heart" is preferred for to_json
    ///                      { ring::heart, "useless" }, // "useless" is interpreted as ring::heart in extract
    ///                    }
    ///                   );
    /// \endcode
    explicit enum_adapter(std::string enum_name, std::initializer_list<std::pair<TEnum, value>> mapping) :
            enum_adapter(std::move(enum_name), mapping.begin(), mapping.end())
    { }

protected:
    virtual TEnum create(const extraction_context& context, const value& from) const override
    {
        using std::end;

        auto iter = _val_to_cpp.find(from);
        if (iter != end(_val_to_cpp))
            return iter->second;
        else
            throw extraction_error(context.path(),
                                   std::string("Invalid value for ") + _enum_name + ": " + to_string(from)
                                  );
    }

    virtual value to_json(const serialization_context&, const TEnum& from) const override
    {
        using std::end;

        auto iter = _cpp_to_val.find(from);
        if (iter != end(_cpp_to_val))
            return iter->second;
        else
            return null;
    }

private:
    std::string                        _enum_name;
    std::map<value, TEnum, FValueComp> _val_to_cpp;
    std::map<TEnum, value, FEnumComp>  _cpp_to_val;
};

/// An adapter for enumeration types which ignores the case when extracting from JSON.
///
/// \see enum_adapter
template <typename TEnum, typename FEnumComp = std::less<TEnum>>
using enum_adapter_icase = enum_adapter<TEnum, FEnumComp, value_less_icase>;

/// \}

}
