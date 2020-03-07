/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/ast.hpp>
#include <jsonv/parse_index.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/reader.hpp>

#include <sstream>

#include "reader_impl.hpp"
#include "reader_impl_parse_index.hpp"

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reader                                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename TImpl, typename... TArgs>
reader::reader(std::in_place_type_t<TImpl>, TArgs&&... args) :
        _impl(std::make_unique<TImpl>(std::forward<TArgs>(args)...))
{ }

reader::reader(parse_index index) :
        reader(std::in_place_type<impl_parse_index>, std::move(index))
{ }

reader::reader(string_view source) :
        reader(std::in_place_type<impl_parse_index>, source)
{ }

reader::reader(string_view source, const parse_options& options) :
        reader(std::in_place_type<impl_parse_index>, source, options)
{ }

reader::reader(const char* source) :
        reader(string_view(source))
{ }

reader::reader(const char* source, const parse_options& options) :
        reader(string_view(source), options)
{ }

reader::reader(std::string&& source) :
        reader(std::in_place_type<impl_parse_index_owning>, std::move(source))
{ }

reader::reader(std::string&& source, const parse_options& options) :
        reader(std::in_place_type<impl_parse_index_owning>, std::move(source), options)
{ }

reader::~reader() noexcept = default;

bool reader::good() const
{
    if (_impl)
        return _impl->good();
    else
        return false;
}

void reader::expect(ast_node_type type)
{
    if (current().type() != type)
    {
        JSONV_UNLIKELY

        std::ostringstream ss;
        ss << "Read node of type " << current().type() << " when expecting " << type;
        throw extraction_error(current_path(), std::move(ss).str());
    }
}

void reader::expect(std::initializer_list<ast_node_type> types)
{
    if (types.size() == 0U)
        throw std::invalid_argument("Cannot expect 0 types");
    else if (types.size() == 1U)
        return expect(*types.begin());

    if (std::find(types.begin(), types.end(), current().type()) == types.end())
    {
        JSONV_UNLIKELY

        std::ostringstream ss;
        ss << "Read node of type " << current().type() << " when expecting one of ";
        for (const auto& type : types)
            ss << type;

        throw extraction_error(current_path(), std::move(ss).str());
    }
}

const ast_node& reader::current() const
{
    if (_impl)
        return _impl->current();
    else
        throw std::invalid_argument("reader instance has been moved-from");
}

const path& reader::current_path() const
{
    if (_impl)
        return _impl->current_path();
    else
        throw std::invalid_argument("reader instance has been moved-from");
}

bool reader::next_token() noexcept
{
    if (_impl)
        return _impl->next_token();
    else
        return false;
}

bool reader::next_structure() noexcept
{
    if (_impl)
        return _impl->next_structure();
    else
        return false;
}

bool reader::next_key() noexcept
{
    if (_impl)
        return _impl->next_key();
    else
        return false;
}

}
