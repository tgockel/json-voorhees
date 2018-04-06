/** \file
 *
 *  Copyright (c) 2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/algorithm.hpp>
#include <jsonv/coerce.hpp>

#include <stdexcept>

namespace jsonv
{

merge_rules::~merge_rules() noexcept = default;

dynamic_merge_rules::dynamic_merge_rules(same_key_function same_key, type_conflict_function type_conflict) :
        same_key(std::move(same_key)),
        type_conflict(std::move(type_conflict))
{ }

dynamic_merge_rules::~dynamic_merge_rules() noexcept = default;

value dynamic_merge_rules::resolve_same_key(path&& current_path, value&& a, value&& b) const
{
    return same_key(std::move(current_path), std::move(a), std::move(b));
}

value dynamic_merge_rules::resolve_type_conflict(path&& current_path, value&& a, value&& b) const
{
    return same_key(std::move(current_path), std::move(a), std::move(b));
}

value throwing_merge_rules::resolve_type_conflict(path&& current_path, value&& a, value&& b) const
{
    throw kind_error(std::string("Found different types at path `") + to_string(current_path) + "': "
                     + to_string(a.kind()) + " and " + to_string(b.kind()));
}

value throwing_merge_rules::resolve_same_key(path&& current_path, value&&, value&&) const
{
    throw std::logic_error(std::string("Cannot merge duplicate key at \"") + to_string(current_path) + "\"");
}

value recursive_merge_rules::resolve_same_key(path&& current_path, value&& a, value&& b) const
{
    return merge_explicit(*this, std::move(current_path), std::move(a), std::move(b));
}

value recursive_merge_rules::resolve_type_conflict(path&&, value&& a, value&& b) const
{
    return coerce_merge(std::move(a), std::move(b));
}

value merge_explicit(const merge_rules& rules,
                     path               current_path,
                     value              a,
                     value              b
                    )
{
    if (  a.kind() != b.kind()
       && !(   (a.kind() == kind::integer && b.kind() == kind::decimal)
            || (a.kind() == kind::decimal && b.kind() == kind::integer)
           )
       )
        return rules.resolve_type_conflict(std::move(current_path), std::move(a), std::move(b));

    switch (a.kind())
    {
        case kind::object:
        {
            value out = object();
            for (value::object_iterator iter = a.begin_object(); iter != a.end_object(); iter = a.erase(iter))
            {
                auto iter_b = b.find(iter->first);
                if (iter_b == b.end_object())
                {
                    out.insert({ iter->first, std::move(iter->second) });
                }
                else
                {
                    out.insert({ iter->first,
                                 rules.resolve_same_key(current_path + iter->first,
                                                        std::move(iter->second),
                                                        std::move(iter_b->second)
                                                       )
                               }
                              );
                    b.erase(iter_b);
                }
            }

            for (value::object_iterator iter = b.begin_object(); iter != b.end_object(); ++iter)
            {
                out.insert({ iter->first, std::move(iter->second) });
            }

            return out;
        }
        case kind::array:
            a.insert(a.end_array(), std::make_move_iterator(b.begin_array()), std::make_move_iterator(b.end_array()));
            return a;
        case kind::boolean:
            return a.as_boolean() || b.as_boolean();
        case kind::integer:
            if (b.kind() == kind::integer)
                return a.as_integer() + b.as_integer();
            // fall through to decimal handler if b is a decimal
        case kind::decimal:
            return a.as_decimal() + b.as_decimal();
        case kind::null:
            return a;
        case kind::string:
            return a.as_string() + b.as_string();
        default:
            throw kind_error(std::string("Invalid kind ") + to_string(a.kind()));
    }
}

value merge_explicit(const merge_rules&, const path&, value a)
{
    return a;
}

value merge_explicit(const merge_rules&, const path&)
{
    return object();
}

}
