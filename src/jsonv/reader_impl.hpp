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
#include <jsonv/value.hpp>

#include <memory>
#include <optional>

namespace jsonv
{

class JSONV_LOCAL reader::impl
{
public:
    /// \param value_backed Whether this source walks an in-memory \c value. See \c value_backed.
    explicit impl(bool value_backed = false);

    virtual ~impl() noexcept;

    /// Does this source walk an in-memory \c value? Only such a source has an answer to \c borrowed_value, or one to
    /// \c current_type that is cheaper than loading the node. Knowing it up front is what lets a reader over text skip
    /// both of those virtual calls on every node, rather than make them to be told there is nothing to gain.
    bool value_backed() const noexcept
    {
        return _value_backed;
    }

    virtual bool good() const = 0;

    /// \see reader::validate
    virtual void validate() const;

    /// \see reader::owns_source
    virtual bool owns_source() const noexcept;

    const ast_node& current() const;

    /// \see reader::current_type
    ast_node_type current_type() const;

    const path& current_path() const;

    /// The in-memory value \c current names, if this source has one to lend. Sources which synthesise their nodes
    /// from text have nothing to return here.
    ///
    /// \see reader::current_value
    virtual optional<const value&> borrowed_value() const noexcept;

    bool next_token();

    bool next_structure();

    bool next_value();

    bool next_key();

    /// Create a second cursor on the node this one is on, which reads the same source without owning it and without
    /// moving this one. It must not outlive this instance.
    ///
    /// \see detail::reader_lookahead
    virtual std::unique_ptr<impl> lookahead() const = 0;

protected:
    /// Attempt to load the current token. If there is no token to load, return \c nullopt.
    virtual std::optional<ast_node> load_current() const = 0;

    /// Get the type of the token \c load_current would load. This is only asked of a \c value_backed source, and only
    /// while \c current is not already cached. Such a source synthesises its nodes' text, so it should answer from
    /// its own state instead -- and must give the same answer \c load_current would. The default loads the node and
    /// asks it.
    ///
    /// \throws std::logic_error if there is no token to load, which is what \c current throws for the same reason.
    virtual ast_node_type load_current_type() const;

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
    const bool                      _value_backed;
    mutable bool                    _current_dirty;
    mutable std::optional<ast_node> _current;
    mutable bool                    _current_path_dirty;
    mutable std::optional<path>     _current_path;
};

namespace detail
{

/// Read ahead of a \c reader without moving it.
///
/// A forward cursor cannot read one subtree twice, and some extractors need to: \c polymorphic_adapter has to find its
/// discriminator, wherever in the object it is, before it knows which type to extract the object as. Rather than rewind
/// the reader, which would make every position-keyed note an extraction leaves about it ambiguous, this opens a second
/// one on the same node.
class JSONV_LOCAL reader_lookahead final
{
public:
    /// Create a reader on the node \a origin is on. It borrows \a origin's source -- even where \a origin owns it -- so
    /// it must not outlive \a origin, although moving \a origin is fine.
    ///
    /// \throws std::invalid_argument if \a origin has been moved-from.
    static reader open(const reader& origin);
};

}

}
