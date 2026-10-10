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

#include <jsonv/config.hpp>
#include <jsonv/parse.hpp>
#include <string_view>

#include <string>
#include <stdexcept>

namespace jsonv::detail
{

class decode_error final :
        public std::runtime_error
{
public:
    typedef std::string::size_type size_type;
public:
    decode_error(size_type offset, const std::string& message);

    virtual ~decode_error() noexcept;

    inline size_type offset() const
    {
        return _offset;
    }

private:
    size_type _offset;
};

/// Encodes C++ string \a source into a fully-escaped JSON string into \a stream ready for sending over the wire. The
/// text is written unformatted, so \a stream's flags, width, fill and locale do not reach it.
std::ostream& string_encode(std::ostream& stream, std::string_view source, bool ensure_ascii = true);

/// A function that decodes an over the wire character sequence \c source into a C++ string.
using string_decode_fn = std::string (*)(std::string_view source);

/// Get a string decoding function for the given output \a encoding.
string_decode_fn get_string_decoder(parse_options::encoding encoding);

/** Convert the UTF-8 encoded \a source into a UTF-16 encoded \c std::wstring. The result is UTF-16 even where
 *  \c wchar_t is 32 bits, so a code point outside the BMP takes a surrogate pair there too.
**/
std::wstring convert_to_wide(std::string_view source);

/// \{
/// Convert the UTF-16 encoded \a source into a UTF-8 encoded \c std::string. As with \c convert_to_wide, \a source is
/// UTF-16 even where \c wchar_t is 32 bits.
///
/// \throws std::range_error if \a source is not valid UTF-16, such as when it holds an unpaired surrogate or a code
///  unit above 0xFFFF.
std::string convert_to_narrow(const std::wstring& source);
std::string convert_to_narrow(const wchar_t*      source);
/// \}

}
