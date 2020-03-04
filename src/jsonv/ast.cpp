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
#include <jsonv/value.hpp>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ostream>
#include <sstream>

#include <iostream>

#include "char_convert.hpp"

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

std::string string_from_token(string_view token, std::true_type is_escaped JSONV_UNUSED)
{
    static const string_decode_fn decoder = get_string_decoder(parse_options::encoding::utf8_strict);

    // chop off the ""s
    token.remove_prefix(1);
    token.remove_suffix(1);

    try
    {
        return decoder(token);
    }
    catch (const decode_error& ex)
    {
        // TODO(#145): This makes more sense as an extraction error
        std::throw_with_nested(parse_error({ { 0, 0, 0, ex.what() } }, null));
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

std::int64_t ast_node::integer::value() const
{
    // TODO(#150): This logic should be moved to a dedicated extractor
    auto characters = token_raw();
    auto end        = const_cast<char*>(characters.data() + characters.size());

    if (characters[0] == '-')
    {
        auto scan_end = end;
        auto val      = std::strtoll(characters.data(), &scan_end, 10);
        if (end == scan_end)
            return val;
        else
            throw make_failed_numeric_extract(*this, "integer");
    }
    else
    {
        // For non-negative integer types, use lexical_cast of a uint64_t then static_cast to an int64_t. This is done
        // to deal with the values 2^63..2^64-1 -- do not consider it an exception, as we can store the bits properly,
        // but the onus is on the user to know the particular key was in the overflow range.
        auto scan_end = end;
        auto val      = std::strtoull(characters.data(), &scan_end, 10);
        if (end == scan_end)
            return val;
        else
            throw make_failed_numeric_extract(*this, "integer");
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ast_node::decimal                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

double ast_node::decimal::value() const
{
    // TODO(#150): This logic should be moved to a dedicated extractor
    auto characters = token_raw();
    auto end        = const_cast<char*>(characters.data() + characters.size());
    auto scan_end   = end;

    auto val = std::strtod(characters.data(), &scan_end);
    if (end == scan_end)
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
    case ast_error::unexpected_token:       return os << "unexpected token";
    case ast_error::eof:                    return os << "input ended unexpectedly";
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
