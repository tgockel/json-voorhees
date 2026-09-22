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
#include <jsonv/path.hpp>
#include <jsonv/reader.hpp>
#include <jsonv/value.hpp>

#include <optional>

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

    /// The in-memory value \c current names, if this source has one to lend. Sources which synthesise their nodes
    /// from text have nothing to return here.
    ///
    /// \see reader::current_value
    virtual const value* borrowed_value() const noexcept;

    bool next_token();

    bool next_structure();

    bool next_value();

    bool next_key();

protected:
    /// Attempt to load the current token. If there is no token to load, return \c nullopt.
    virtual std::optional<ast_node> load_current() const = 0;

    virtual std::optional<path> load_current_path() const = 0;

    virtual bool next_token_impl() noexcept = 0;

    virtual bool next_structure_impl() noexcept;

    /// Step over the value \c current is on. There is no depth-counting default here on purpose: the only source
    /// today is \c impl_parse_index, which reads the structure's extent off the tape, so a default would be code
    /// nothing runs. A source which cannot do better should grow one when it arrives.
    virtual bool next_value_impl() noexcept = 0;

    virtual bool next_key_impl() noexcept;

    /// Mark the cache as dirty. This should be used any time the token changes. It is automatically called from the
    /// \c next_X functions.
    void mark_dirty();

private:
    mutable bool                    _current_dirty;
    mutable std::optional<ast_node> _current;
    mutable bool                    _current_path_dirty;
    mutable std::optional<path>     _current_path;
};

}
