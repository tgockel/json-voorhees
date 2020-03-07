/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization.hpp>

#include "detail/overload.hpp"
#include "reader_impl_parse_index.hpp"

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reader::impl_parse_index                                                                                           //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

reader::impl_parse_index::impl_parse_index(parse_index source) :
        _index(std::move(source)),
        _current(_index.begin())
{ }

reader::impl_parse_index::impl_parse_index(string_view source, const parse_options& options) :
        impl_parse_index(parse_index::parse(source, options))
{ }

reader::impl_parse_index::impl_parse_index(string_view source) :
        impl_parse_index(parse_index::parse(source))
{ }

reader::impl_parse_index::~impl_parse_index() noexcept = default;

bool reader::impl_parse_index::good() const
{
    return _current != _index.end();
}

optional<ast_node> reader::impl_parse_index::load_current() const
{
    if (good())
        return *_current;
    else
        return nullopt;
}

static path build_current_path(parse_index::const_iterator begin, parse_index::const_iterator current)
{
    if (begin == current)
        return path();

    auto end = current;
    ++end;

    std::vector<std::pair<ast_node, std::size_t>> elem_stack;

    for (auto iter = begin; iter != end; ++iter)
    {
        const auto& node = *iter;
        auto        tok  = node.type();

        if (!elem_stack.empty() && elem_stack.back().first.type() == ast_node_type::array_begin)
            ++elem_stack.back().second;

        if (tok == ast_node_type::object_begin || tok == ast_node_type::array_begin)
        {
            elem_stack.emplace_back(node, std::size_t(0U) - 1);
        }
        else if (tok == ast_node_type::object_end || tok == ast_node_type::array_end)
        {
            elem_stack.pop_back();
        }
        else if (tok == ast_node_type::key_canonical || tok == ast_node_type::key_escaped)
        {
            elem_stack.back().first = node;
        }
    }

    path::storage_type elements;
    elements.reserve(elem_stack.size());
    for (const auto& [node, idx] : elem_stack)
    {
        if (node.type() == ast_node_type::key_canonical || node.type() == ast_node_type::key_escaped)
        {
            elements.push_back(node.visit_key([](const auto& x) { return std::string(x.value()); }));
        }
        else if (node.type() == ast_node_type::array_begin)
        {
            // If we see -1, that means we haven't visited any elements of the array yet. The path at this point should
            // still represent the whole array, not the first element of it.
            if (idx != (std::size_t(0U) - 1))
                elements.push_back(idx);
        }
    }

    return path(std::move(elements));
}

optional<path> reader::impl_parse_index::load_current_path() const
{
    if (good())
        return build_current_path(_index.begin(), _current);
    else
        return nullopt;
}

bool reader::impl_parse_index::next_token_impl() noexcept
{
    if (_current == _index.end())
        return false;

    ++_current;
    return _current != _index.end();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reader::impl_parse_index_owning                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

reader::impl_parse_index_owning::impl_parse_index_owning(std::string&& source) :
        reader_impl_parse_index_owning_string(std::move(source)),
        reader::impl_parse_index(this->source)
{ }

reader::impl_parse_index_owning::impl_parse_index_owning(std::string&& source, const parse_options& options) :
        reader_impl_parse_index_owning_string(std::move(source)),
        reader::impl_parse_index(this->source, options)
{ }

reader::impl_parse_index_owning::~impl_parse_index_owning() noexcept = default;

}
