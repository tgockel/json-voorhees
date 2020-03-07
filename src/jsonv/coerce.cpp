/** \file
 *
 *  Copyright (c) 2014-2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/coerce.hpp>
#include <jsonv/algorithm.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/value.hpp>

#include <limits>

#include "detail/fallthrough.hpp"

namespace jsonv
{

bool can_coerce(const kind& from, const kind& to)
{
    switch (to)
    {
    case kind::null:
    case kind::object:
    case kind::array:
        // object, array and null cannot be coerced to, so the kinds must match
        return from == to;
    case kind::string:
    case kind::boolean:
        return true;
    case kind::decimal:
    case kind::integer:
        return from == kind::decimal || from == kind::integer;
    default:
        // can't coerce to a corrupt kind
        return false;
    }
}

bool can_coerce(const value& from, const kind& to)
{
    if (can_coerce(from.kind(), to))
    {
        return true;
    }
    else if (from.kind() == kind::string && (to == kind::decimal || to == kind::integer))
    {
        // Actually attempt the conversion from string into the proper number. If it succeeds, we can coerce the string.
        try
        {
            if (to == kind::decimal)
                coerce_decimal(from);
            else
                coerce_integer(from);
            return true;
        }
        catch (const kind_error&)
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

std::nullptr_t coerce_null(const value& from)
{
    if (from.kind() == kind::null)
        return nullptr;
    else
        throw kind_error(std::string("Can only coerce null from a null, but from is of kind ")
                         + to_string(from.kind())
                        );
}

std::map<std::string, value> coerce_object(const value& from)
{
    if (from.kind() == kind::object)
        return std::map<std::string, value>(from.begin_object(), from.end_object());
    else
        throw kind_error(std::string("Invalid kind for object: ") + to_string(from.kind()));
}

std::vector<value> coerce_array(const value& from)
{
    if (from.kind() == kind::array)
        return std::vector<value>(from.begin_array(), from.end_array());
    else
        throw kind_error(std::string("Invalid kind for array: ") + to_string(from.kind()));
}

std::string coerce_string(const value& from)
{
    if (from.kind() == kind::string)
        return from.as_string();
    else
        return to_string(from);
}

std::string coerce_string(reader& from)
{
    const auto& current = from.current();
    switch (current.type())
    {
    case ast_node_type::string_canonical:
        return std::string(current.as<ast_node::string_canonical>().value());
    case ast_node_type::string_escaped:
        return current.as<ast_node::string_escaped>().value();
    case ast_node_type::key_canonical:
        return std::string(current.as<ast_node::key_canonical>().value());
    case ast_node_type::key_escaped:
        return current.as<ast_node::key_escaped>().value();
    case ast_node_type::literal_true:
    case ast_node_type::literal_false:
    case ast_node_type::literal_null:
    case ast_node_type::integer:
    case ast_node_type::decimal:
        return std::string(current.token_raw());
    default:
        throw kind_error(std::string("Invalid AST node type for string: ") + to_string(current.type()));
    }
}

std::int64_t coerce_integer(const value& from)
{
    switch (from.kind())
    {
    case kind::boolean:
        return from.as_boolean() ? 1 : 0;
    case kind::integer:
        return from.as_integer();
    case kind::decimal:
        if (from.as_decimal() > double(std::numeric_limits<std::int64_t>::max()))
            return std::numeric_limits<std::int64_t>::max();
        else
            return std::int64_t(from.as_decimal());
    case kind::string:
        try
        {
            value x = parse(from.as_string());
            if (x.kind() == kind::integer || x.kind() == kind::decimal || x.kind() == kind::null)
                return coerce_integer(x);
        }
        catch (const parse_error&)
        { }
        throw kind_error(std::string("Could not interpret string ") + to_string(from) + " as an integer.");
    case kind::null:
    case kind::object:
    case kind::array:
    default:
        throw kind_error(std::string("Invalid kind for integer: ") + to_string(from.kind()));
    }
}

double coerce_decimal(const value& from)
{
    switch (from.kind())
    {
    case kind::boolean:
        return from.as_boolean() ? 1.0 : 0.0;
    case kind::integer:
    case kind::decimal:
        return from.as_decimal();
    case kind::string:
        try
        {
            value x = parse(from.as_string());
            if (x.kind() == kind::integer || x.kind() == kind::decimal || x.kind() == kind::null)
                return x.as_decimal();
        }
        catch (const parse_error&)
        { }
        throw kind_error(std::string("Could not interpret string ") + to_string(from) + " as a decimal.");
    case kind::null:
    case kind::object:
    case kind::array:
    default:
        throw kind_error(std::string("Invalid kind for decimal: ") + to_string(from.kind()));
    }
}

double coerce_decimal(reader& from)
{
    const auto& current = from.current();
    switch (current.type())
    {
    case ast_node_type::literal_true:
        return 1.0;
    case ast_node_type::literal_false:
        return 0.0;
    case ast_node_type::integer:
        return static_cast<double>(current.as<ast_node::integer>().value());
    case ast_node_type::decimal:
        return current.as<ast_node::decimal>().value();
    case ast_node_type::string_canonical:
    case ast_node_type::string_escaped:
        return coerce_decimal(value(coerce_string(from)));
    default:
        throw kind_error(std::string("Invalid AST node type for decimal: ") + to_string(current.type()));
    }
}

bool coerce_boolean(const value& from)
{
    switch (from.kind())
    {
    case kind::null:
        return false;
    case kind::object:
    case kind::array:
    case kind::string:
        return !from.empty();
    case kind::integer:
        return from != 0;
    case kind::decimal:
        return from != 0.0;
    case kind::boolean:
        return from.as_boolean();
    default:
        throw kind_error(std::string("Invalid kind for boolean: ") + to_string(from.kind()));
    }
}

bool coerce_boolean(reader& from)
{
    const auto& current = from.current();
    switch (current.type())
    {
    case ast_node_type::literal_null:
    case ast_node_type::literal_false:
        return false;
    case ast_node_type::literal_true:
        return true;
    case ast_node_type::object_begin:
        return current.as<ast_node::object_begin>().element_count() != 0U;
    case ast_node_type::array_begin:
        return current.as<ast_node::array_begin>().element_count() != 0U;
    case ast_node_type::string_canonical:
    case ast_node_type::string_escaped:
    case ast_node_type::key_canonical:
    case ast_node_type::key_escaped:
        return !coerce_string(from).empty();
    case ast_node_type::integer:
        return current.as<ast_node::integer>().value() != 0;
    case ast_node_type::decimal:
        return current.as<ast_node::decimal>().value() != 0.0;
    default:
        throw kind_error(std::string("Invalid AST node type for boolean: ") + to_string(current.type()));
    }
}

value coerce_merge(value a, value b)
{
    if (a.kind() == b.kind())
        return merge_recursive(std::move(a), std::move(b));
    else if (b.kind() == kind::null)
        return a;
    else switch (a.kind())
    {
    case kind::array:
        a.push_back(std::move(b));
        return a;
    case kind::boolean:
        return a.as_boolean() || coerce_boolean(b);
    case kind::integer:
        if (can_coerce(b, kind::integer))
            return a.as_integer() + coerce_integer(b);
        JSONV_FALLTHROUGH();
    case kind::decimal:
        if (can_coerce(b, kind::decimal))
            return a.as_decimal() + coerce_decimal(b);
        else
            return coerce_merge(std::move(b), std::move(a));
    case kind::null:
        return b;
    case kind::object:
        a["undefined"] = std::move(b);
        return a;
    case kind::string:
        return a.as_string() + coerce_string(b);
    default:
        throw kind_error(to_string(a.kind()));
    }
}

}
