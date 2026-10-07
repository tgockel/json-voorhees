/** \file
 *  Classes and functions for encoding JSON values to various representations.
 *
 *  Copyright (c) 2014 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/encode.hpp>
#include <jsonv/value.hpp>

#include "detail.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// encoder                                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

encoder::~encoder() noexcept = default;

void encoder::encode(const value& source)
{
    write_tree(source);
}

void encoder::write_tree(const value& source)
{
    switch (source.kind())
    {
    case kind::array:
        write_array_begin();
        {
            bool first = true;
            for (const value& sub : source.as_array())
            {
                if (first)
                    first = false;
                else
                    write_array_delimiter();
                write_tree(sub);
            }
        }
        write_array_end();
        break;
    case kind::boolean:
        write_boolean(source.as_boolean());
        break;
    case kind::decimal:
        write_decimal(source.as_decimal());
        break;
    case kind::integer:
        write_integer(source.as_integer());
        break;
    case kind::null:
        write_null();
        break;
    case kind::object:
        write_object_begin();
        {
            bool first = true;
            for (const value::object_value_type& entry : source.as_object())
            {
                if (first)
                    first = false;
                else
                    write_object_delimiter();

                write_object_key(entry.first);
                write_tree(entry.second);
            }
        }
        write_object_end();
        break;
    case kind::string:
        write_string(source.as_string());
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ostream_encoder                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ostream_encoder::ostream_encoder(std::ostream& output) :
        _output(output),
        _ensure_ascii(true)
{ }

ostream_encoder::~ostream_encoder() noexcept = default;

void ostream_encoder::ensure_ascii(bool value)
{
    _ensure_ascii = value;
}

void ostream_encoder::write_array_begin()
{
    _output << '[';
}

void ostream_encoder::write_array_end()
{
    _output << ']';
}

void ostream_encoder::write_array_delimiter()
{
    _output << ',';
}

void ostream_encoder::write_boolean(bool value)
{
    _output << (value ? "true" : "false");
}

void ostream_encoder::write_decimal(double value)
{
    if (std::isfinite(value))
    {
        std::array<char, number_token_max> buffer;
        if (auto text = format_decimal(value, buffer.data()))
            _output.write(text->data(), static_cast<std::streamsize>(text->size()));
        else
            _output << value;
    }
    else
        // non-finite values do not have valid JSON representations, so put it as null
        write_null();
}

void ostream_encoder::write_integer(std::int64_t value)
{
    _output << value;
}

void ostream_encoder::write_null()
{
    _output << "null";
}

void ostream_encoder::write_object_begin()
{
    _output << '{';
}

void ostream_encoder::write_object_end()
{
    _output << '}';
}

void ostream_encoder::write_object_delimiter()
{
    _output << ',';
}

void ostream_encoder::write_object_key(std::string_view key)
{
    write_string(key);
    _output << ':';
}

void ostream_encoder::write_string(std::string_view value)
{
    stream_escaped_string(_output, value, _ensure_ascii);
}

std::ostream& ostream_encoder::output()
{
    return _output;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ostream_pretty_encoder                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ostream_pretty_encoder::ostream_pretty_encoder(std::ostream& output, std::size_t indent_size) :
        ostream_encoder(output),
        _indent(0),
        _indent_size(indent_size),
        _defer_indent(false)
{ }

ostream_pretty_encoder::~ostream_pretty_encoder() noexcept = default;

void ostream_pretty_encoder::write_prefix()
{
    if (_defer_indent)
    {
        write_eol();
        _defer_indent = false;
    }
}

void ostream_pretty_encoder::write_eol()
{
    output() << '\n';
    for (std::size_t x = 0; x < _indent; ++x)
        output() << ' ';
}

void ostream_pretty_encoder::write_array_begin()
{
    write_prefix();
    ostream_encoder::write_array_begin();
    _indent += _indent_size;
    _defer_indent = true;
}

void ostream_pretty_encoder::write_array_end()
{
    _indent -= _indent_size;
    if (!_defer_indent)
    {
        write_eol();
    }
    _defer_indent = false;
    ostream_encoder::write_array_end();
}

void ostream_pretty_encoder::write_array_delimiter()
{
    write_prefix();
    ostream_encoder::write_array_delimiter();
    write_eol();
}

void ostream_pretty_encoder::write_boolean(bool value)
{
    write_prefix();
    ostream_encoder::write_boolean(value);
}

void ostream_pretty_encoder::write_decimal(double value)
{
    write_prefix();
    ostream_encoder::write_decimal(value);
}

void ostream_pretty_encoder::write_integer(std::int64_t value)
{
    write_prefix();
    ostream_encoder::write_integer(value);
}

void ostream_pretty_encoder::write_null()
{
    write_prefix();
    ostream_encoder::write_null();
}

void ostream_pretty_encoder::write_object_begin()
{
    write_prefix();
    ostream_encoder::write_object_begin();
    _indent += _indent_size;
    _defer_indent = true;
}

void ostream_pretty_encoder::write_object_end()
{
    _indent -= _indent_size;
    if (!_defer_indent)
    {
        write_eol();
    }
    _defer_indent = false;
    ostream_encoder::write_object_end();
}

void ostream_pretty_encoder::write_object_delimiter()
{
    ostream_encoder::write_object_delimiter();
    _defer_indent = true;
}

void ostream_pretty_encoder::write_object_key(std::string_view key)
{
    write_prefix();
    ostream_encoder::write_object_key(key);
    output() << ' ';
}

void ostream_pretty_encoder::write_string(std::string_view value)
{
    write_prefix();
    ostream_encoder::write_string(value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// value_encoder                                                                                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// The state behind a \c value_encoder: the structures being built and the document finished so far.
///
/// Nothing here checks the grammar. The hooks are reachable only through a \c writer, which refuses a token the
/// grammar does not allow before the sink sees it, and through \c encoder::write_tree, whose source is a \c value and
/// so is well-formed by construction. A hook therefore trusts that it is called where its token belongs: a key
/// arrives with an object open, a value inside an object arrives after a key, and an end arrives with a structure to
/// close.
///
/// A hook which fails -- and only running out of memory can fail one -- leaves the tree as it was before the call,
/// which is what the writer assumes of a sink, since it updates its own state only once the hook has returned. The
/// one place that takes care is closing a structure, which moves the finished container out of its frame straight
/// into its parent and pops the frame only afterwards: a parent which cannot take it leaves the child still open, and
/// still whole, on both sides.
class JSONV_LOCAL value_encoder::impl final
{
public:
    /// Open \a container -- an empty object or array -- as the innermost structure.
    void begin(value&& container)
    {
        _open.push_back(frame{ std::move(container), std::string() });
    }

    /// Close the innermost structure, placing it in its parent.
    void end()
    {
        frame& child = _open.back();
        if (_open.size() == 1U)
            finish(std::move(child.container));
        else
            store(_open[_open.size() - 2U], std::move(child.container));
        _open.pop_back();
    }

    /// Keep \a key for the next value placed in the innermost object.
    void key(std::string_view key)
    {
        _open.back().key.assign(key);
    }

    /// Place the scalar \a val where the next value goes.
    void put(value&& val)
    {
        if (_open.empty())
            finish(std::move(val));
        else
            store(_open.back(), std::move(val));
    }

    value take()
    {
        if (!_open.empty())
            throw std::logic_error("Cannot take the value while the document is incomplete: "
                                   + std::to_string(_open.size()) + " structure(s) still open");
        if (!_has_root)
            throw std::logic_error("Cannot take the value: nothing has been written");

        _has_root = false;
        return std::exchange(_root, value());
    }

private:
    /// One structure being built.
    struct frame
    {
        /// The object or array.
        value       container;
        /// For an object: the key of the member whose value comes next.
        std::string key;
    };

    /// \a val is a whole document. A document already held is replaced: the encoder holds one at a time, and the last
    /// written is it, as the last of a repeated key is.
    void finish(value&& val) noexcept
    {
        _root     = std::move(val);
        _has_root = true;
    }

    /// Place \a val in \a into: as its next element, or as the value of the key waiting in it. Assignment rather than
    /// `insert`, which would keep the *first* of a repeated key: `parse_index::extract_tree` and `read_value` both
    /// keep the last under the default `duplicate_key_action::replace`, and a tree built from tokens disagreeing with
    /// `parse` about which of `{"x":1,"x":2}` survives would make the two select different data. Both `push_back` and
    /// `operator[]` allocate before they move, so running out of memory here leaves \a val and \a into as they were.
    static void store(frame& into, value&& val)
    {
        if (into.container.kind() == kind::array)
            into.container.push_back(std::move(val));
        else
            into.container[std::move(into.key)] = std::move(val);
    }

private:
    /// The structures being built, innermost last.
    std::vector<frame> _open;
    value              _root;
    bool               _has_root = false;
};

value_encoder::value_encoder() :
        _impl(std::make_unique<impl>())
{ }

value_encoder::~value_encoder() noexcept = default;

value value_encoder::take() &&
{
    return _impl->take();
}

void value_encoder::write_null()
{
    _impl->put(value());
}

void value_encoder::write_object_begin()
{
    _impl->begin(object());
}

void value_encoder::write_object_end()
{
    _impl->end();
}

void value_encoder::write_object_key(std::string_view key)
{
    _impl->key(key);
}

void value_encoder::write_object_delimiter()
{ }

void value_encoder::write_array_begin()
{
    _impl->begin(array());
}

void value_encoder::write_array_end()
{
    _impl->end();
}

void value_encoder::write_array_delimiter()
{ }

void value_encoder::write_string(std::string_view value)
{
    _impl->put(jsonv::value(value));
}

void value_encoder::write_integer(std::int64_t value)
{
    _impl->put(jsonv::value(value));
}

void value_encoder::write_decimal(double value)
{
    _impl->put(jsonv::value(value));
}

void value_encoder::write_boolean(bool value)
{
    _impl->put(jsonv::value(value));
}

}
