/// \file
///
/// Copyright (c) 2025 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "reader_impl_value.hpp"

#include <jsonv/path.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <utility>

#include "detail.hpp"

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reader_token_arena                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

constexpr std::size_t arena_chunk_size = 4096U;

}

reader_token_arena::reader_token_arena() noexcept :
        _next(nullptr),
        _remaining(0U)
{ }

reader_token_arena::~reader_token_arena() noexcept = default;

char* reader_token_arena::allocate(std::size_t size)
{
    // The `_next == nullptr` half is what makes a zero-byte request still yield a real address, since a view over one
    // is only well-formed if its pointer is.
    if (size > _remaining || _next == nullptr)
    {
        auto chunk_size = std::max(size, arena_chunk_size);
        auto chunk      = std::make_unique<char[]>(chunk_size);

        // Push before adopting the chunk: if the vector reallocates and throws, the arena is still pointing at
        // storage it owns rather than at a buffer which has just been freed.
        _chunks.push_back(std::move(chunk));
        _next      = _chunks.back().get();
        _remaining = chunk_size;
    }

    char* out = _next;
    _next      += size;
    _remaining -= size;
    return out;
}

std::string_view reader_token_arena::intern(std::string_view text)
{
    char* out = allocate(text.size());
    if (!text.empty())
        std::memcpy(out, text.data(), text.size());

    return std::string_view(out, text.size());
}

std::string_view reader_token_arena::intern_quoted(std::string_view text)
{
    char* out = allocate(text.size() + 2U);

    out[0U]                = '"';
    out[text.size() + 1U]  = '"';
    if (!text.empty())
        std::memcpy(out + 1, text.data(), text.size());

    return std::string_view(out, text.size() + 2U);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reader::impl_value                                                                                                 //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

/// Every token which is not a number, a string or a key has fixed text, so all of them can be views into this. Laid
/// out so each one is a contiguous run: `{` `}` `[` `]` at 0-3, `true` at 4, `false` at 8, `null` at 13, and the
/// zero-length `document_start` / `document_end` token at the terminator.
constexpr char fixed_token_text[] = "{}[]truefalsenull";

constexpr const char* token_object_begin = fixed_token_text + 0;
constexpr const char* token_object_end   = fixed_token_text + 1;
constexpr const char* token_array_begin  = fixed_token_text + 2;
constexpr const char* token_array_end    = fixed_token_text + 3;
constexpr const char* token_true         = fixed_token_text + 4;
constexpr const char* token_false        = fixed_token_text + 8;
constexpr const char* token_null         = fixed_token_text + 13;
constexpr const char* token_empty        = fixed_token_text + 17;

// Pin the offsets: the alternative is a silently wrong token for anyone who edits the string above.
static_assert(*token_object_begin == '{');
static_assert(*token_object_end   == '}');
static_assert(*token_array_begin  == '[');
static_assert(*token_array_end    == ']');
static_assert(std::string_view(token_true,  4U) == "true");
static_assert(std::string_view(token_false, 5U) == "false");
static_assert(std::string_view(token_null,  4U) == "null");
static_assert(token_empty == std::end(fixed_token_text) - 1);

constexpr bool is_structure(jsonv::kind k) noexcept
{
    return k == jsonv::kind::object || k == jsonv::kind::array;
}

}

reader::impl_value::impl_value(const value& source) noexcept :
        _root(&source),
        _position(position::document_start),
        _close_type(ast_node_type::document_end)
{ }

reader::impl_value::~impl_value() noexcept = default;

bool reader::impl_value::good() const
{
    return _position != position::exhausted;
}

const value* reader::impl_value::borrowed_value() const noexcept
{
    // `at_value` is the only position naming a whole value: a key names half a member, a close names a structure the
    // cursor has already left, and the two document positions sit outside the tree entirely.
    if (_position != position::at_value)
        return nullptr;
    else
        return &current_value();
}

const value& reader::impl_value::current_value() const noexcept
{
    if (_stack.empty())
        return *_root;

    const frame& top = _stack.back();
    if (top.container->kind() == jsonv::kind::object)
        return top.member->second;
    else
        return (*top.container)[top.index];
}

void reader::impl_value::descend() noexcept
{
    const value& container  = current_value();
    const bool   is_object  = container.kind() == jsonv::kind::object;
    const auto   size       = container.size();

    if (size == 0U)
    {
        // No frame at all for an empty structure: the very next token is the matching close, and pushing a frame only
        // to pop it again on the following step would be the same answer for more work.
        _close_type = is_object ? ast_node_type::object_end : ast_node_type::array_end;
        _position   = position::at_close;
        return;
    }

    frame next{ &container, value::const_object_iterator(), value::const_object_iterator(), 0U, 0U };
    if (is_object)
    {
        next.member     = container.begin_object();
        next.member_end = container.end_object();
    }
    else
    {
        next.size = size;
    }

    try
    {
        _stack.push_back(next);
    }
    catch (...)
    {
        // The frame stack is the only allocation on the `noexcept` side of this reader. Report it the way the rest of
        // the `next_` family reports failure: turning a case nobody can recover from into a `std::terminate` is the
        // same bad trade #213 was about.
        _position = position::exhausted;
        return;
    }

    _position = is_object ? position::at_key : position::at_value;
}

void reader::impl_value::advance() noexcept
{
    if (_stack.empty())
    {
        _position = position::document_end;
        return;
    }

    frame& top = _stack.back();
    if (top.container->kind() == jsonv::kind::object)
    {
        ++top.member;
        if (top.member == top.member_end)
            (void) close_frame();
        else
            _position = position::at_key;
    }
    else
    {
        ++top.index;
        if (top.index == top.size)
            (void) close_frame();
        else
            _position = position::at_value;
    }
}

bool reader::impl_value::step_over_structure(jsonv::kind container_kind) noexcept
{
    // Nothing inside is visited and no frame is touched, so stepping over a structure costs the same no matter how
    // large it is -- the tree-walking answer to the tape's `skip_subtree`.
    //
    // The landing spot is one *past* the matching close rather than on it, because what is being stepped over is the
    // whole value. A reader left sitting on the `}` of a member it skipped looks to an object loop exactly like the
    // end of the enclosing object, so every member after it would be silently dropped -- and only for this source,
    // which is worse than being wrong everywhere. `skip_subtree` lands in the same place.
    _close_type = (container_kind == jsonv::kind::object) ? ast_node_type::object_end : ast_node_type::array_end;
    _position   = position::at_close;
    return next_token_impl();
}

bool reader::impl_value::close_frame() noexcept
{
    if (_stack.empty())
        return false;

    _close_type = _stack.back().container->kind() == jsonv::kind::object ? ast_node_type::object_end
                                                                        : ast_node_type::array_end;
    _stack.pop_back();
    _position = position::at_close;
    return true;
}

ast_node reader::impl_value::load_value_node(const value& source) const
{
    switch (source.kind())
    {
    case jsonv::kind::object:
        return ast_node(std::in_place_type<ast_node::object_begin>, token_object_begin, source.size());
    case jsonv::kind::array:
        return ast_node(std::in_place_type<ast_node::array_begin>, token_array_begin, source.size());
    case jsonv::kind::null:
        return ast_node(std::in_place_type<ast_node::literal_null>, token_null);
    case jsonv::kind::boolean:
        if (source.as_boolean())
            return ast_node(std::in_place_type<ast_node::literal_true>, token_true);
        else
            return ast_node(std::in_place_type<ast_node::literal_false>, token_false);
    case jsonv::kind::string:
    {
        // A `value`'s string bytes are already decoded, so this is always the canonical node type and no escape
        // decoding ever runs on this path. Note that a string containing a `"` therefore produces a token which is
        // not well-formed JSON on its own -- that is safe only because nothing re-lexes it, and the canonical
        // `detail::string_from_token` strips exactly the outer two bytes.
        auto text = _arena.intern_quoted(source.as_string());
        return ast_node(std::in_place_type<ast_node::string_canonical>, text.data(), text.size());
    }
    case jsonv::kind::integer:
    {
        char buffer[number_token_max];
        if (auto text = format_integer(source.as_integer(), buffer))
        {
            auto token = _arena.intern(*text);
            return ast_node(std::in_place_type<ast_node::integer>, token.data(), token.size());
        }
        break;
    }
    case jsonv::kind::decimal:
    {
        auto decimal = source.as_decimal();
        if (std::isfinite(decimal))
        {
            char buffer[number_token_max];
            if (auto text = format_decimal(decimal, buffer))
            {
                auto token = _arena.intern(*text);
                return ast_node(std::in_place_type<ast_node::decimal>, token.data(), token.size());
            }
        }
        break;
    }
    }

    // A number with no JSON representation. That is reachable for a non-finite `double` and, in principle, for a
    // `to_chars` failure which `number_token_max` makes impossible. `null` is what the encoder writes for the former,
    // so a value-sourced reader and an encoded document agree about what a NaN looks like.
    return ast_node(std::in_place_type<ast_node::literal_null>, token_null);
}

std::optional<ast_node> reader::impl_value::load_current() const
{
    switch (_position)
    {
    case position::document_start:
        return ast_node(std::in_place_type<ast_node::document_start>, token_empty);
    case position::at_value:
        return load_value_node(current_value());
    case position::at_key:
    {
        auto text = _arena.intern_quoted(_stack.back().member->first);
        return ast_node(std::in_place_type<ast_node::key_canonical>, text.data(), text.size());
    }
    case position::at_close:
        if (_close_type == ast_node_type::object_end)
            return ast_node(std::in_place_type<ast_node::object_end>, token_object_end);
        else
            return ast_node(std::in_place_type<ast_node::array_end>, token_array_end);
    case position::document_end:
        return ast_node(std::in_place_type<ast_node::document_end>, token_empty);
    case position::exhausted:
    default:
        return std::nullopt;
    }
}

std::optional<path> reader::impl_value::load_current_path() const
{
    if (!good())
        return std::nullopt;

    // The stack *is* the path. A frame is pushed only once the reader has stepped off the opening token and popped
    // as it steps onto the closing one, which is exactly the "the path refers to the entire array again" rule the
    // `reader` documentation spells out -- so there is nothing to special-case here, and no walk from the start of
    // the document the way a `parse_index` source needs.
    path::storage_type elements;
    elements.reserve(_stack.size());
    for (const frame& f : _stack)
    {
        if (f.container->kind() == jsonv::kind::object)
            elements.emplace_back(std::string_view(f.member->first));
        else
            elements.emplace_back(f.index);
    }

    return path(std::move(elements));
}

bool reader::impl_value::next_token_impl() noexcept
{
    switch (_position)
    {
    case position::document_start:
        // With no frames, `current_value` is the root -- which is the only value not inside a container.
        _position = position::at_value;
        break;
    case position::at_key:
        _position = position::at_value;
        break;
    case position::at_value:
        if (is_structure(current_value().kind()))
            descend();
        else
            advance();
        break;
    case position::at_close:
        advance();
        break;
    case position::document_end:
        _position = position::exhausted;
        break;
    case position::exhausted:
        break;
    }

    return good();
}

bool reader::impl_value::next_value_impl() noexcept
{
    if (!good())
        return false;

    if (_position == position::at_value)
    {
        if (auto k = current_value().kind(); is_structure(k))
            return step_over_structure(k);
    }

    return next_token_impl();
}

bool reader::impl_value::next_structure_impl() noexcept
{
    // The depth-counting default would give the same answers, but it reaches them by calling `current` on every node
    // it steps over -- which synthesises a token, and therefore allocates arena, for every node being skipped.
    if (!good())
        return false;

    switch (_position)
    {
    case position::at_close:
    case position::document_end:
        // There is no longer a structure to leave, so this is merely `next_token`.
        return next_token_impl();
    case position::at_value:
        // Sitting on an opener, the structure to leave is the one it opens -- which is the same thing `next_value`
        // does from here. The two only diverge on a key or a scalar.
        if (auto k = current_value().kind(); is_structure(k))
            return step_over_structure(k);
        break;
    default:
        break;
    }

    if (close_frame())
        return next_token_impl();

    // No enclosing structure means the document was never structurally enclosed -- neither `{ ... }` nor `[ ... ]`.
    // Fall back to the end of the document, which is what the base does for the same case.
    _position = position::document_end;
    return true;
}

bool reader::impl_value::next_key_impl() noexcept
{
    // `reader::impl::next_key` has already rejected everything but a key and an opening `{`, so there are only two
    // cases to serve; anything else is a caller the base let through and is reported as failure rather than walked.
    if (_position == position::at_value && current_value().kind() == jsonv::kind::object)
        descend();
    else if (_position == position::at_key)
        advance();
    else
        return false;

    return good();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// reader::impl_value_owning                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

reader::impl_value_owning::impl_value_owning(value&& source) :
        reader_impl_value_owning_value(std::move(source)),
        reader::impl_value(this->source)
{ }

reader::impl_value_owning::~impl_value_owning() noexcept = default;

}
