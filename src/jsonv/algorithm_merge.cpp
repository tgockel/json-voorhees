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

#include "detail/fallthrough.hpp"
#include "detail/integer_sum.hpp"

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
    return type_conflict(std::move(current_path), std::move(a), std::move(b));
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
            // Both objects iterate in key order, so walking them side by side visits every key once and produces
            // `out` in order: each insert is hinted at the end, where it lands.
            value out = object();
            auto  iter_a = a.begin_object();
            auto  iter_b = b.begin_object();
            while (iter_a != a.end_object() || iter_b != b.end_object())
            {
                if (iter_b == b.end_object() || (iter_a != a.end_object() && iter_a->first < iter_b->first))
                {
                    out.insert(out.end_object(), { iter_a->first, std::move(iter_a->second) });
                    ++iter_a;
                }
                else if (iter_a == a.end_object() || iter_b->first < iter_a->first)
                {
                    out.insert(out.end_object(), { iter_b->first, std::move(iter_b->second) });
                    ++iter_b;
                }
                else
                {
                    out.insert(out.end_object(),
                               { iter_a->first,
                                 rules.resolve_same_key(current_path + iter_a->first,
                                                        std::move(iter_a->second),
                                                        std::move(iter_b->second)
                                                       )
                               }
                              );
                    ++iter_a;
                    ++iter_b;
                }
            }

            return out;
        }
        case kind::array:
            a.insert(a.end_array(), std::make_move_iterator(b.begin_array()), std::make_move_iterator(b.end_array()));
            return a;
        case kind::boolean:
            return a.as_boolean() || b.as_boolean();
        case kind::integer:
            if (b.kind() == kind::integer && detail::integer_sum_fits(a.as_integer(), b.as_integer()))
                return a.as_integer() + b.as_integer();
            // fall through to decimal handler if b is a decimal or the sum does not fit in an integer
            JSONV_FALLTHROUGH();
        case kind::decimal:
            return a.as_decimal() + b.as_decimal();
        case kind::null:
            return a;
        case kind::string:
            return std::string(a.as_string()).append(b.as_string());
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
