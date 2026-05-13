/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "reader_impl.hpp"

#include <sstream>
#include <stdexcept>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reader::impl                                                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

reader::impl::impl() :
        _current_dirty(true),
        _current_path_dirty(true)
{ }

reader::impl::~impl() noexcept = default;

const ast_node& reader::impl::current() const
{
    if (_current_dirty)
    {
        _current = load_current();
        _current_dirty = false;
    }

    if (_current)
        return *_current;
    else
        throw std::logic_error("Cannot get current -- did `next` or `skip` return false?");
}

const path& reader::impl::current_path() const
{
    if (_current_path_dirty)
    {
        _current_path       = load_current_path();
        _current_path_dirty = false;
    }

    if (_current_path)
        return *_current_path;
    else
        throw std::logic_error("Cannot get current path -- did `next` or `skip` return false?");
}

void reader::impl::mark_dirty()
{
    _current_dirty      = true;
    _current            = nullopt;
    _current_path_dirty = true;
    _current_path       = nullopt;
}

bool reader::impl::next_token()
{
    mark_dirty();
    return next_token_impl();
}

bool reader::impl::next_structure()
{
    mark_dirty();
    return next_structure_impl();
}

bool reader::impl::next_key()
{
    ast_node_type current_type;
    try
    {
        current_type = current().type();
    }
    catch (...)
    {
        return false;
    }

    if (  current_type != ast_node_type::key_canonical
       && current_type != ast_node_type::key_escaped
       && current_type != ast_node_type::object_begin
       )
    {
        std::ostringstream ss;
        ss << "Cannot call next_key for non-key AST node type " << current().type();
        throw std::invalid_argument(std::move(ss).str());
    }

    mark_dirty();
    return next_key_impl();
}

bool reader::impl::next_structure_impl() noexcept
{
    // If we start at the end of a structure, then just go to the next thing
    if (auto tok = current().type();
        tok == ast_node_type::object_end || tok == ast_node_type::array_end
       )
    {
        return next_token();
    }

    std::size_t depth = 1U;
    while (next_token())
    {
        auto tok = current().type();
        if (tok == ast_node_type::object_end || tok == ast_node_type::array_end)
        {
            --depth;
            if (depth == 0U)
                return next_token();
        }
        else if (tok == ast_node_type::object_begin || tok == ast_node_type::array_begin)
        {
            ++depth;
        }
        // If we find the end of the document at depth of 1, we were in a JSON document that was not structurally
        // enclosed -- niether { ... } nor [ ... ]. Fall back to using the document ending as the structure.
        else if (tok == ast_node_type::document_end && depth == 1U)
        {
            return true;
        }
    }
    return false;
}

bool reader::impl::next_key_impl() noexcept
{
    if (!next_token())
        return false;

    if (auto tok = current().type();
        (  tok == ast_node_type::key_canonical
        || tok == ast_node_type::key_escaped
        || tok == ast_node_type::object_end
        )
       )
    {
        return true;
    }

    std::size_t depth = 0U;
    do
    {
        auto tok = current().type();
        if (tok == ast_node_type::object_end || tok == ast_node_type::array_end || tok == ast_node_type::document_end)
        {
            --depth;
        }
        else if (tok == ast_node_type::object_begin || tok == ast_node_type::array_begin)
        {
            ++depth;
        }

        if (depth == 0U)
            return next_token();
    } while (next_token());
    return false;
}

}
