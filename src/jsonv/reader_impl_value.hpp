/// \file
///
/// Copyright (c) 2025 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/value.hpp>

#include <memory>
#include <string_view>
#include <vector>

#include <cstddef>

#include "reader_impl.hpp"

namespace jsonv
{

/// Storage for the token text a \c reader::impl_value synthesises.
///
/// An \c ast_node is a \c const \c char* and a length, so a reader over a \c value -- which holds numbers as
/// \c std::int64_t / \c double and strings as decoded bytes -- has to write token text somewhere. That somewhere has
/// to hand out **stable** addresses: a \c std::string_view taken from \c reader::current has to stay readable after
/// \c reader::next_token, because that is what a \c parse_index -sourced reader provides for free by pointing into
/// the source text. Two reader sources disagreeing about it would be an unpleasant bug to find in an extractor.
///
/// So this is a bump allocator over chunks which are never reused or freed while it lives. Only numbers, strings and
/// keys need it -- every other token is a view into a static string -- and the base memoizes \c load_current, so it
/// grows by at most one token per node actually visited. A walk which skips a subtree never pays for it.
class JSONV_LOCAL reader_token_arena final
{
public:
    explicit reader_token_arena() noexcept;

    reader_token_arena(const reader_token_arena&)            = delete;
    reader_token_arena& operator=(const reader_token_arena&) = delete;

    ~reader_token_arena() noexcept;

    /// Copy \a text into the arena, returning a view of the copy.
    std::string_view intern(std::string_view text);

    /// Copy \a text into the arena surrounded by `"`, returning a view of the whole thing -- which is the token
    /// representation of a JSON string.
    std::string_view intern_quoted(std::string_view text);

    /// Get \a size writable bytes.
    char* allocate(std::size_t size);

private:
    /// Chunks are `unique_ptr`s rather than `std::string`s so that growing the vector moves pointers and not buffers.
    std::vector<std::unique_ptr<char[]>> _chunks;
    char*                                _next;
    std::size_t                          _remaining;
};

/// A \c reader which walks an in-memory \c value.
///
/// \see reader::from_value
class JSONV_LOCAL reader::impl_value :
        public reader::impl
{
public:
    explicit impl_value(const value& source) noexcept;

    virtual ~impl_value() noexcept override;

    virtual bool good() const override;

    virtual std::optional<ast_node> load_current() const override;

    virtual std::optional<path> load_current_path() const override;

    virtual bool next_token_impl() noexcept override;

    virtual bool next_value_impl() noexcept override;

    virtual bool next_structure_impl() noexcept override;

    virtual bool next_key_impl() noexcept override;

private:
    /// One open structure. The cursor is the member or element this frame is currently *on*, which is also what it
    /// contributes to \c current_path. The bounds are captured when the frame is pushed so that stepping it touches
    /// nothing which could throw -- \c value::end_object and \c value::size both check the kind first, and the
    /// stepping happens inside \c noexcept functions.
    struct frame
    {
        const value*                 container;
        value::const_object_iterator member;      //!< object: the current member
        value::const_object_iterator member_end;  //!< object: one past the last member
        std::size_t                  index;       //!< array: the current element
        std::size_t                  size;        //!< array: the element count
    };

    /// Where the cursor sits. The token the reader is on is a function of this plus the top of \c _stack.
    enum class position : std::uint8_t
    {
        document_start,
        at_value,       //!< on the opening `{` / `[` or the scalar token of the current value
        at_key,         //!< on the key of the top frame's current member
        at_close,       //!< on `}` / `]` -- the frame it closes has already been popped
        document_end,
        exhausted,      //!< past the end; `good` is false
    };

private:
    /// The value the cursor is on in \c position::at_value. With no frames that is the root, since the root is the
    /// only value not inside a container.
    const value& current_value() const noexcept;

    /// Build the node for \a source, synthesising its token text into the arena if it needs any.
    ast_node load_value_node(const value& source) const;

    /// Step into the container the cursor is on, having just reported its opening token.
    void descend() noexcept;

    /// Step the innermost frame past the value just finished, closing it if that was the last one.
    void advance() noexcept;

    /// Step past the matching close of the structure the cursor is on the opener of, without visiting anything
    /// inside it. \c next_value and \c next_structure mean the same thing from an opener, so they share this.
    bool step_over_structure(jsonv::kind container_kind) noexcept;

    /// Leave the innermost frame, landing on its close token. Reports whether there was a frame to leave.
    bool close_frame() noexcept;

private:
    const value*              _root;
    std::vector<frame>        _stack;
    position                  _position;
    /// Which close token \c position::at_close is on, since its frame is gone by then.
    ast_node_type             _close_type;
    mutable reader_token_arena _arena;
};

/// Holds the \c value a \c reader::from_value(value&&) was given. Listed before \c reader::impl_value in the base
/// list of \c impl_value_owning so it is alive before the impl which points into it -- the same ordering trick
/// \c reader_impl_parse_index_owning_string plays for a source string.
struct JSONV_LOCAL reader_impl_value_owning_value
{
    value source;

    explicit reader_impl_value_owning_value(value&& source) :
            source(std::move(source))
    { }
};

class JSONV_LOCAL reader::impl_value_owning :
        private reader_impl_value_owning_value,
        public reader::impl_value
{
public:
    explicit impl_value_owning(value&& source);

    virtual ~impl_value_owning() noexcept override;
};

}
