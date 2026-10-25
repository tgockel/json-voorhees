/// \file
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/value.hpp>

#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "array_impl.hpp"
#include "object_impl.hpp"
#include "string_impl.hpp"

namespace jsonv::detail
{

/// Lets library code reach past the public interface of \c value: to read and write a string in whichever of its two
/// layouts it is in, and to fill an array or object's container directly, without the kind check and the out-of-line
/// call each \c push_back or \c operator[] costs.
struct JSONV_LOCAL value_access final
{
    /// Make \a out, which must be a \c kind::null, a \c kind::string holding a copy of \a text: in itself if it is
    /// short enough, otherwise in a block of its own. If this throws, \a out is unchanged.
    static void set_string(value& out, std::string_view text)
    {
        if (text.size() <= value_inline_string_capacity)
        {
            value_short_string short_string{};
            short_string.kind          = jsonv::kind::string;
            short_string.string_length = static_cast<unsigned char>(text.size());
            if (!text.empty())
                std::memcpy(short_string.text, text.data(), text.size());
            out._repr.short_string = short_string;
        }
        else
        {
            out._repr.words = { jsonv::kind::string, value_string_out_of_line, { .string = string_impl::make(text) } };
        }
    }

    /// Make a \c kind::string holding a copy of \a text.
    static value string(std::string_view text)
    {
        value out;
        set_string(out, text);
        return out;
    }

    /// The text of \a str, which must be a \c kind::string.
    static std::string_view string_view(const value& str) noexcept
    {
        if (str._repr.words.string_length == value_string_out_of_line)
            return str._repr.words.data.string->view();
        else
            return std::string_view(str._repr.short_string.text, str._repr.short_string.string_length);
    }

    /// Make an empty \c kind::array with room for \a capacity elements.
    static value array(value::size_type capacity)
    {
        value out;
        out._repr.words.data.array = array_impl::allocate(capacity);
        out._repr.words.kind       = jsonv::kind::array;
        return out;
    }

    /// Add \a item to the end of \a array, which must be a \c kind::array.
    static void array_push_back(value& array, value&& item)
    {
        array_impl::emplace_back(array._repr.words.data.array, std::move(item));
    }

    /// The pointer to the members of \a object, which must be a \c kind::object -- for an \c object_builder to fill.
    static object_impl*& object_storage(value& object) noexcept
    {
        return object._repr.words.data.object;
    }
};

}
