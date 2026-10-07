/// \file
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/encode.hpp>
#include <jsonv/path.hpp>
#include <jsonv/value.hpp>
#include <jsonv/writer.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// writer::impl                                                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// The state behind a \c writer: the sink and the stack of open structures.
///
/// The frames below \c _depth are the open structures, innermost last. The frames at and above it are closed ones kept
/// for their storage: a frame's \c key is a \c std::string which is reused for every member of every object which
/// later occupies that slot, so once a document has been written, writing another of the same shape allocates nothing.
///
/// Every token checks the grammar first, then writes to the sink, then updates the state. A token the grammar refuses
/// therefore reaches the sink with nothing, and a sink which throws leaves the state describing what it was given
/// before the call.
class JSONV_LOCAL writer::impl final
{
public:
    explicit impl(encoder& to) :
            _to(&to)
    { }

    explicit impl(std::ostream& to) :
            _to(&_owned.emplace(to))
    { }

    impl(const impl&)            = delete;
    impl& operator=(const impl&) = delete;

    std::size_t depth() const noexcept
    {
        return _depth;
    }

    const path& current_path() const
    {
        if (_path_dirty)
        {
            path built;
            for (std::size_t idx = 0U; idx < _depth; ++idx)
            {
                const frame& open = _frames[idx];
                if (open.container == frame::shape::array)
                    built += path_element(open.count);
                else if (open.key_pending)
                    built += path_element(std::string_view(open.key));
                // An object with no key pending contributes nothing: the next token is a key, which has no slot yet.
                // Only the innermost frame can be in this state, since a structure opens inside an object only once
                // a key has named it, and that key waits until the structure closes.
            }

            _path       = std::move(built);
            _path_dirty = false;
        }
        return _path;
    }

    void object_begin()
    {
        before_value();
        frame& opened = reserve_frame();
        _to->write_object_begin();
        open(opened, frame::shape::object);
    }

    void object_end()
    {
        const frame& top = innermost("Cannot end an object: nothing is open");
        if (top.container != frame::shape::object)
            throw std::logic_error("Cannot end an object while inside an array");
        if (top.key_pending)
            throw std::logic_error("Cannot end an object while the member \"" + top.key
                                   + "\" is waiting for its value");

        _to->write_object_end();
        close();
    }

    void array_begin()
    {
        before_value();
        frame& opened = reserve_frame();
        _to->write_array_begin();
        open(opened, frame::shape::array);
    }

    void array_end()
    {
        const frame& top = innermost("Cannot end an array: nothing is open");
        if (top.container != frame::shape::array)
            throw std::logic_error("Cannot end an array while inside an object");

        _to->write_array_end();
        close();
    }

    void key(std::string_view key)
    {
        frame& top = innermost("Cannot write a key outside of an object");
        if (top.container != frame::shape::object)
            throw std::logic_error("Cannot write a key inside an array");
        if (top.key_pending)
            throw std::logic_error("Cannot write a key while the member \"" + top.key
                                   + "\" is waiting for its value");

        // Kept before anything is written, so that running out of memory here leaves the sink untouched.
        top.key.assign(key);

        if (top.count > 0U)
            _to->write_object_delimiter();
        _to->write_object_key(key);

        top.key_pending = true;
        _path_dirty     = true;
    }

    void null()
    {
        before_value();
        _to->write_null();
        after_value();
    }

    void boolean(bool value)
    {
        before_value();
        _to->write_boolean(value);
        after_value();
    }

    void integer(std::int64_t value)
    {
        before_value();
        _to->write_integer(value);
        after_value();
    }

    void decimal(double value)
    {
        before_value();
        _to->write_decimal(value);
        after_value();
    }

    void string(std::string_view value)
    {
        before_value();
        _to->write_string(value);
        after_value();
    }

    /// One grammar check for the slot the whole tree fills. What becomes of the tree is for the encoder to decide,
    /// through the same \c encoder::write_tree hook \c encode calls.
    void write(const value& source)
    {
        before_value();
        _to->write_tree(source);
        after_value();
    }

    void write(value&& source)
    {
        before_value();
        _to->write_tree(std::move(source));
        after_value();
    }

private:
    /// One open structure.
    struct frame
    {
        enum class shape : unsigned char
        {
            array,
            object,
        };

        shape       container   = shape::array;
        /// The elements or members written so far. For an array, this is also the index of the next element.
        std::size_t count       = 0U;
        /// Has a key been written whose value has not? Only meaningful for an object.
        bool        key_pending = false;
        /// The key of the member being written, for \c current_path and for error messages. Only meaningful for an
        /// object, and only while \c key_pending.
        std::string key;
    };

    /// The innermost open structure.
    ///
    /// \throws std::logic_error carrying \a message if nothing is open.
    frame& innermost(const char* message)
    {
        if (_depth == 0U)
            throw std::logic_error(message);

        return _frames[_depth - 1U];
    }

    /// Check that a value may be written at the current slot, and write the delimiter which separates it from the
    /// value before it. A member's delimiter is written with its key, since that is what comes first.
    void before_value()
    {
        if (_depth == 0U)
            return;

        const frame& top = _frames[_depth - 1U];
        if (top.container == frame::shape::object)
        {
            if (!top.key_pending)
                throw std::logic_error("Cannot write a value inside an object without a key before it");
        }
        else if (top.count > 0U)
        {
            _to->write_array_delimiter();
        }
    }

    /// Note that the value at the current slot has been written in full.
    void after_value() noexcept
    {
        _path_dirty = true;
        if (_depth == 0U)
            return;

        frame& top      = _frames[_depth - 1U];
        top.key_pending = false;
        ++top.count;
    }

    /// The frame the next structure will occupy, grown into existence if it has to be. This can throw, which is why it
    /// is asked for before the opening token is written rather than after.
    frame& reserve_frame()
    {
        if (_depth == _frames.size())
            _frames.emplace_back();

        return _frames[_depth];
    }

    /// Make \a opened, which \c reserve_frame gave, the innermost open structure. Its \c key keeps whatever storage it
    /// had: it is assigned before it is read.
    void open(frame& opened, frame::shape container) noexcept
    {
        opened.container   = container;
        opened.count       = 0U;
        opened.key_pending = false;

        ++_depth;
        _path_dirty = true;
    }

    /// Pop the innermost structure, whose closing token has been written, and count it as a value in its parent.
    void close() noexcept
    {
        --_depth;
        after_value();
    }

private:
    /// The encoder behind the \c std::ostream constructor. Declared before \c _to, which points into it.
    std::optional<ostream_encoder> _owned;
    encoder*                       _to;
    std::vector<frame>             _frames;
    std::size_t                    _depth      = 0U;
    mutable path                   _path;
    mutable bool                   _path_dirty = true;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// writer                                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

/// The state behind a writer, or the complaint a moved-from one makes.
template <typename TImpl>
TImpl& state_of(const std::unique_ptr<TImpl>& impl)
{
    if (!impl)
        throw std::invalid_argument("writer instance has been moved-from");

    return *impl;
}

}

writer::writer(encoder& to) :
        _impl(std::make_unique<impl>(to))
{ }

writer::writer(std::ostream& to) :
        _impl(std::make_unique<impl>(to))
{ }

writer::writer(writer&&) noexcept            = default;
writer& writer::operator=(writer&&) noexcept = default;

writer::~writer() noexcept = default;

bool writer::good() const noexcept
{
    return bool(_impl);
}

std::size_t writer::depth() const noexcept
{
    if (_impl)
        return _impl->depth();
    else
        return 0U;
}

const path& writer::current_path() const
{
    return state_of(_impl).current_path();
}

writer& writer::object_begin()
{
    state_of(_impl).object_begin();
    return *this;
}

writer& writer::object_end()
{
    state_of(_impl).object_end();
    return *this;
}

writer& writer::array_begin()
{
    state_of(_impl).array_begin();
    return *this;
}

writer& writer::array_end()
{
    state_of(_impl).array_end();
    return *this;
}

writer& writer::key(std::string_view key)
{
    state_of(_impl).key(key);
    return *this;
}

writer& writer::null()
{
    state_of(_impl).null();
    return *this;
}

writer& writer::boolean(bool value)
{
    state_of(_impl).boolean(value);
    return *this;
}

writer& writer::integer(std::int64_t value)
{
    state_of(_impl).integer(value);
    return *this;
}

writer& writer::decimal(double value)
{
    state_of(_impl).decimal(value);
    return *this;
}

writer& writer::string(std::string_view value)
{
    state_of(_impl).string(value);
    return *this;
}

writer& writer::write(const value& source)
{
    state_of(_impl).write(source);
    return *this;
}

writer& writer::write(value&& source)
{
    state_of(_impl).write(std::move(source));
    return *this;
}

}
