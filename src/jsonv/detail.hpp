/// \file
///
/// Copyright (c) 2012-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/ast.hpp>
#include <jsonv/value.hpp>
#include <optional>
#include <string_view>

#include <cstddef>
#include <cstdint>

namespace jsonv
{

const char* kind_desc(kind type);
bool kind_valid(kind k);
void check_type(kind expected, kind actual);
void check_type(std::initializer_list<kind> expected, kind actual);
std::ostream& stream_escaped_string(std::ostream& stream, std::string_view str, bool require_ascii);

/// The largest token \c format_decimal or \c format_integer can produce, including room for the `.0` fixup. Both
/// take a \c buffer with at least this much space.
inline constexpr std::size_t number_token_max = 64U;

/// \{
/// Format \a value as the JSON token text for a number, writing into \a buffer and returning a view of what was
/// written. These are the single definition of what a number looks like in JSON Voorhees: \c ostream_encoder writes
/// through them, and \c reader::impl_value synthesises its tokens with them, so the text an encoded document carries
/// and the text a value-sourced reader hands an extractor cannot drift apart.
///
/// \param buffer Storage of at least \c number_token_max bytes. The returned view points into it.
/// \returns A view of \a buffer holding the token; \c nullopt if \c std::to_chars failed. That cannot happen for a
///          buffer of the documented size, but the caller decides what to do about it rather than this deciding for
///          them.
///
/// \pre \c std::isfinite(value) -- a non-finite \c double has no JSON representation at all, so the choice of what
///      to write instead belongs to the caller.
std::optional<std::string_view> format_decimal(double value, char* buffer);
std::optional<std::string_view> format_integer(std::int64_t value, char* buffer);
/// \}

/// \{
/// \c ast_node::expect asked of a node type rather than a node. \c reader::expect answers through here with
/// \c reader::current_type, which a value-backed reader can give without synthesising the node's token text.
///
/// \throws std::invalid_argument if \a expected_types is empty.
std::expected<void, ast_node_type> expect_node_type(ast_node_type found_type, ast_node_type expected_type);
std::expected<void, ast_node_type> expect_node_type(ast_node_type                        found_type,
                                                    std::initializer_list<ast_node_type> expected_types
                                                   );
/// \}

/// The \c value an integer token is read as -- the single definition of it for everything which builds a \c value
/// from JSON text, which is \c parse and a \c reader over text. A literal from 2^63 through 2^64-1 keeps its bits as
/// \c ast_node::integer::value() does, and one beyond the range of 64 bits altogether is the nearest \c double.
///
/// \throws std::invalid_argument if the literal has no finite \c double to round to either.
value integer_node_value(const ast_node::integer& node);

}
