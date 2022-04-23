/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/parse_index.hpp>

#include "reader_impl.hpp"

namespace jsonv
{

class JSONV_LOCAL reader::impl_parse_index :
        public reader::impl
{
public:
    explicit impl_parse_index(string_view source);
    explicit impl_parse_index(string_view source, const parse_options& options);

    explicit impl_parse_index(parse_index source);

    virtual ~impl_parse_index() noexcept override;

    virtual bool good() const override;

    virtual optional<ast_node> load_current() const override;

    virtual optional<path> load_current_path() const override;

    virtual bool next_token_impl() noexcept override;

private:
    parse_index                 _index;
    parse_index::const_iterator _current;
};

struct JSONV_LOCAL reader_impl_parse_index_owning_string
{
    std::string source;

    explicit reader_impl_parse_index_owning_string(std::string&& source) :
            source(std::move(source))
    { }
};

class JSONV_LOCAL reader::impl_parse_index_owning :
        private reader_impl_parse_index_owning_string,
        public reader::impl_parse_index
{
public:
    explicit impl_parse_index_owning(std::string&& source);
    explicit impl_parse_index_owning(std::string&& source, const parse_options& options);

    virtual ~impl_parse_index_owning() noexcept override;
};

}
