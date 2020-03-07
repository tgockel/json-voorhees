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

#include <jsonv/ast.hpp>
#include <jsonv/optional.hpp>
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>

namespace jsonv
{

class JSONV_LOCAL reader::impl
{
public:
    explicit impl();

    virtual ~impl() noexcept;

    virtual bool good() const = 0;

    const ast_node& current() const;

    const path& current_path() const;

    bool next_token();

    bool next_structure();

    bool next_key();

protected:
    /// Attempt to load the current token. If there is no token to load, return \c nullopt.
    virtual optional<ast_node> load_current() const = 0;

    virtual optional<path> load_current_path() const = 0;

    virtual bool next_token_impl() noexcept = 0;

    virtual bool next_structure_impl() noexcept;

    virtual bool next_key_impl() noexcept;

    /// Mark the cache as dirty. This should be used any time the token changes. It is automatically called from the
    /// \c next_X functions.
    void mark_dirty();

private:
    mutable bool               _current_dirty;
    mutable optional<ast_node> _current;
    mutable bool               _current_path_dirty;
    mutable optional<path>     _current_path;
};

}
