/// \file
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/ast.hpp>
#include <jsonv/parse.hpp>
#include <jsonv/serialization.hpp>
#include <jsonv/value.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>

#include <iostream>
#include <system_error>

#include "char_convert.hpp"
#include "detail/fast_float/fast_float.h"

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Private Helper Functions                                                                                           //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static inline char char_repr(ast_node_type src /* UNSAFE */)
{
    static const char reprs[] = "$^{}[]sSkKtfnid!";
    return reprs[static_cast<std::uint8_t>(src)];
}

static inline std::invalid_argument make_failed_numeric_extract(const ast_node& src, const char* type)
{
    std::ostringstream ss;
    ss << "Failed to extract " << type << " from \"" << src.token_raw() << "\"";
    return std::invalid_argument(std::move(ss).str());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Helper Functions                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace detail
{

std::string string_from_token(std::string_view token, std::true_type is_escaped JSONV_UNUSED)
{
    // TODO(#150): This logic should be in a dedicated extractor
    static const string_decode_fn decoder = get_string_decoder(parse_options::encoding::utf8);

    // chop off the ""s
    token.remove_prefix(1);
    token.remove_suffix(1);

    try
    {
        return decoder(token);
    }
    catch (const decode_error& ex)
    {
        // While this occurs during extraction, this is technically a parse error -- we should not have accepted the
        // source JSON into the AST
        throw parse_error(ex.what());
    }
}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ast_node_type                                                                                                      //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const ast_node_type& type)
{
    auto repr_type = static_cast<std::uint8_t>(type);
    auto repr_max  = static_cast<std::uint8_t>(ast_node_type::error);

    if (repr_type <= repr_max)
        os << char_repr(type);
    else
        os << "Invalid type value: " << +repr_type;
    return os;
}

std::string to_string(const ast_node_type& type)
{
    std::ostringstream ss;
    ss << type;
    return std::move(ss).str();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ast_node::literal_null                                                                                             //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

jsonv::value ast_node::literal_null::value() const
{
    return jsonv::null;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ast_node::integer                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Parse 8 ASCII decimal digits at `chars[0..7]` into a 32-bit result. Caller must guarantee every
// byte is in '0'..'9'. Classic SWAR trick (Wojciech Mula, popularized by simdjson): mask off the
// high nibble to get the digit value of each byte in place, then pairwise-combine digits with
// multiplies whose constants encode (10*256+1), (100*65536+1), (10000*2^32+1).
static inline std::uint32_t parse_eight_digits_swar(const char* chars) noexcept
{
    std::uint64_t val;
    std::memcpy(&val, chars, sizeof val);
    val = (val & 0x0F0F0F0F0F0F0F0FULL) * 2561ULL >> 8;
    val = (val & 0x00FF00FF00FF00FFULL) * 6553601ULL >> 16;
    return static_cast<std::uint32_t>((val & 0x0000FFFF0000FFFFULL) * 42949672960001ULL >> 32);
}

// Parse a run of ASCII digits in [p, end) as an unsigned integer. Caller must guarantee every byte
// is in '0'..'9' and that the value fits in uint64 (i.e. end - p <= 19 digits).
static inline std::uint64_t parse_unsigned_integer_swar(const char* p, const char* end) noexcept
{
    std::uint64_t result = 0;
    while (end - p >= 8)
    {
        result = result * 100000000ULL + parse_eight_digits_swar(p);
        p += 8;
    }
    while (p < end)
    {
        result = result * 10ULL + static_cast<std::uint64_t>(*p - '0');
        ++p;
    }
    return result;
}

std::int64_t ast_node::integer::value() const
{
    // TODO(#150): This logic should be moved to a dedicated extractor
    auto characters = token_raw();
    auto begin      = characters.data();
    auto end        = characters.data() + characters.size();

    const bool negative          = (*begin == '-');
    const auto magnitude_begin   = negative ? begin + 1 : begin;
    const auto magnitude_digits  = static_cast<std::size_t>(end - magnitude_begin);

    // SWAR fast path is safe when the magnitude provably fits without overflowing int64 (negative
    // path) or uint64 (positive path):
    //   - negative magnitudes >= 19 digits may exceed |INT64_MIN|, where strtoll clamps to LLONG_MIN
    //   - positive magnitudes >= 20 digits may exceed UINT64_MAX, where strtoull clamps to ULLONG_MAX
    // Fall back to the strto* slow path in those corners so the existing overflow semantics are
    // preserved bit-for-bit.
    const std::size_t swar_limit = negative ? 18 : 19;
    if (magnitude_digits <= swar_limit)
    {
        const auto magnitude = parse_unsigned_integer_swar(magnitude_begin, end);
        return negative
             ? -static_cast<std::int64_t>(magnitude)
             :  static_cast<std::int64_t>(magnitude);
    }

    auto scan_end = const_cast<char*>(end);
    if (negative)
    {
        auto val = std::strtoll(begin, &scan_end, 10);
        if (scan_end == end)
            return val;
    }
    else
    {
        // For non-negative integer types, use lexical_cast of a uint64_t then static_cast to an
        // int64_t. This is done to deal with the values 2^63..2^64-1 -- do not consider it an
        // exception, as we can store the bits properly, but the onus is on the user to know the
        // particular key was in the overflow range.
        auto val = std::strtoull(begin, &scan_end, 10);
        if (scan_end == end)
            return static_cast<std::int64_t>(val);
    }
    throw make_failed_numeric_extract(*this, "integer");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ast_node::decimal                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double ast_node::decimal::value() const
{
    // TODO(#150): This logic should be moved to a dedicated extractor
    auto characters = token_raw();
    auto begin      = characters.data();
    auto end        = characters.data() + characters.size();

    // fast_float implements the Eisel-Lemire fast-path with a correct slow-path
    // fallback for ambiguous cases. The JSON grammar (validated upstream in
    // match_number) is a strict subset of `chars_format::general`, so we don't
    // need to enable hex / infinity / nan parsing.
    double val{};
    auto result = fast_float::from_chars(begin, end, val, fast_float::chars_format::general);
    if (  result.ptr == end
       && (  result.ec == std::errc{}
          || (result.ec == std::errc::result_out_of_range && val == 0.0)
          )
       )
        return val;
    else
        throw make_failed_numeric_extract(*this, "decimal");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ast_error                                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const ast_error& src)
{
    switch (src)
    {
    case ast_error::none:                   return os << "none";
    case ast_error::expected_document:      return os << "expected document (object or array)";
    case ast_error::unexpected_token:       return os << "unexpected token";
    case ast_error::unexpected_eof:         return os << "input ended unexpectedly";
    case ast_error::expected_eof:           return os << "extra characters in input";
    case ast_error::depth_exceeded:         return os << "max structural depth exceeded";
    case ast_error::extra_close:            return os << "extra closing character";
    case ast_error::mismatched_close:       return os << "mismatched closing character";
    case ast_error::close_after_comma:      return os << "structure closed after comma";
    case ast_error::unexpected_comma:       return os << "unexpected comma";
    case ast_error::expected_string:        return os << "expected a string";
    case ast_error::expected_key_delimiter: return os << "expected ':'";
    case ast_error::invalid_literal:        return os << "invalid literal";
    case ast_error::invalid_number:         return os << "invalid number format";
    case ast_error::invalid_string:         return os << "invalid string format";
    case ast_error::invalid_comment:        return os << "invalid comment block";
    case ast_error::internal:               return os << "internal parser error";
    default:                                return os << "ast_error(" << static_cast<std::uint64_t>(src) << ")";
    }
}

std::string to_string(const ast_error& src)
{
    std::ostringstream ss;
    ss << src;
    return std::move(ss).str();
}

}
