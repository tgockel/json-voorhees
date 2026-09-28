/// \file
///
/// Copyright (c) 2022 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/ast.hpp>
#include <jsonv/parse_index.hpp>
#include <jsonv/reader.hpp>

#include <stdexcept>

#include "detail.hpp"
#include "reader_impl.hpp"
#include "reader_impl_parse_index.hpp"
#include "reader_impl_value.hpp"

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

reader reader::from_value(const value& source)
{
    return reader(std::in_place_type<impl_value>, source);
}

reader reader::from_value(value&& source)
{
    return reader(std::in_place_type<impl_value_owning>, std::move(source));
}

reader::reader(std::string_view source) :
        reader(std::in_place_type<impl_parse_index>, source)
{ }

reader::reader(std::string_view source, const parse_options& options) :
        reader(std::in_place_type<impl_parse_index>, source, options)
{ }

reader::reader(const char* source) :
        reader(std::string_view(source))
{ }

reader::reader(const char* source, const parse_options& options) :
        reader(std::string_view(source), options)
{ }

reader::reader(std::string&& source) :
        reader(std::in_place_type<impl_parse_index_owning>, std::move(source))
{ }

reader::reader(std::string&& source, const parse_options& options) :
        reader(std::in_place_type<impl_parse_index_owning>, std::move(source), options)
{ }

reader::reader(reader&&) noexcept            = default;
reader& reader::operator=(reader&&) noexcept = default;

reader::~reader() noexcept = default;

bool reader::good() const
{
    if (_impl)
        return _impl->good();
    else
        return false;
}

void reader::validate() const
{
    if (_impl)
        _impl->validate();
    else
        throw std::invalid_argument("reader instance has been moved-from");
}

bool reader::owns_source() const noexcept
{
    if (_impl)
        return _impl->owns_source();
    else
        return false;
}

std::expected<void, ast_node_type> reader::expect(ast_node_type type) const
{
    return expect_node_type(current_type(), type);
}

std::expected<void, ast_node_type> reader::expect(std::initializer_list<ast_node_type> types) const
{
    // `current_type` first: a reader which is not `good` is the more fundamental mistake, so it is the one reported
    // when a caller has made both.
    auto found = current_type();
    return expect_node_type(found, types);
}

const ast_node& reader::current() const
{
    if (_impl)
        return _impl->current();
    else
        throw std::invalid_argument("reader instance has been moved-from");
}

ast_node_type reader::current_type() const
{
    if (_impl)
        return _impl->current_type();
    else
        throw std::invalid_argument("reader instance has been moved-from");
}

const value* reader::current_value() const noexcept
{
    // Asked on every scalar an extractor reads, so a reader over text answers without the virtual call.
    if (_impl && _impl->value_backed())
        return _impl->borrowed_value();
    else
        return nullptr;
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

bool reader::next_value() noexcept
{
    if (_impl)
        return _impl->next_value();
    else
        return false;
}

bool reader::next_key()
{
    if (_impl)
        return _impl->next_key();
    else
        return false;
}

}
