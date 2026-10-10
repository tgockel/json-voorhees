/** \file
 *
 *  Copyright (c) 2012-2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "char_convert.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cwchar>
#include <iomanip>
#include <locale>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#include "detail/fixed_map.hpp"
#include "detail/is_print.hpp"
#include "detail/utf8.hpp"

namespace jsonv
{
namespace detail
{

decode_error::decode_error(size_type offset, const std::string& message):
        runtime_error(message),
        _offset(offset)
{ }

decode_error::~decode_error() noexcept
{ }

/** The short escapes the encoder writes. **/
#define ENCODED_ESCAPES_LIST(item) \
    item('\b', 'b')  \
    item('\f', 'f')  \
    item('\n', 'n')  \
    item('\r', 'r')  \
    item('\t', 't')  \
    item('\\', '\\') \
    item('\"', '\"') \

/** The short escapes the decoder reads: the ones the encoder writes, and \c \\/. RFC 8259 lets a string spell a solidus
 *  that way, so the decoder has to understand it. Nothing requires it, though -- only a quote, a backslash and the
 *  control characters must be escaped -- so the encoder writes \c / as it is.
**/
#define ESCAPES_LIST(item) \
    ENCODED_ESCAPES_LIST(item) \
    item('/',  '/')  \

#define TUPLE_PLUS_1_GEN(a, b) +1 // NOLINT(bugprone-macro-parentheses): each expansion is one term of a sum
typedef detail::fixed_map<char, char, ENCODED_ESCAPES_LIST(TUPLE_PLUS_1_GEN)> encode_converter_map;
typedef detail::fixed_map<char, char, ESCAPES_LIST(TUPLE_PLUS_1_GEN)>         decode_converter_map;

/** These entries are sorted by the numeric value of the ASCII character (\c less_entry_cpp).
 *
 *  \note
 *  The encode and decode map must be in a different order (even though they share most of their data) because the
 *  ASCII representations of escape sequences are not in the same order as the characters they are escaping.
**/
#define TUPLE_FIRST_SECOND(a, b) { a, b },
const encode_converter_map encode_map = { ENCODED_ESCAPES_LIST(TUPLE_FIRST_SECOND) };

/** These entries are sorted by the character value of the escape sequence (\c less_entry_json).
**/
#define TUPLE_SECOND_FIRST(a, b) { b, a },
const decode_converter_map decode_map = { ESCAPES_LIST(TUPLE_SECOND_FIRST) };

template <typename TConverterMap>
const char* find(const TConverterMap& source, char key)
{
    typename TConverterMap::const_iterator iter = source.find(key);
    if (iter != source.end())
        return &iter->second;
    else
        return NULL;
}

static const char* find_encoding(char native_character)
{
    return find(encode_map, native_character);
}

static const char* find_decoding(char char_after_backslash)
{
    return find(decode_map, char_after_backslash);
}

static bool needs_unicode_escaping(char c)
{
    return bool(c & '\x80')
        || !is_print(c);
}

/** The codepoint encoded by the \a length bytes at \a seq, which must be a sequence \c utf8_sequence_length allows. **/
static char32_t utf8_decode(const char* seq, unsigned length)
{
    // The bits of the lead byte which belong to the codepoint, by sequence length.
    static constexpr unsigned char lead_masks[] = { 0x00U, 0x7fU, 0x1fU, 0x0fU, 0x07U };

    char32_t code = static_cast<unsigned char>(seq[0]) & lead_masks[length];
    for (unsigned idx = 1; idx < length; ++idx)
        code = (code << 6) | (static_cast<unsigned char>(seq[idx]) & 0x3fU);

    return code;
}

static const char hex_codes[] = "0123456789abcdef";

static void to_hex(std::ostream& stream, uint16_t code)
{
    for (int pos = 3; pos >= 0; --pos)
    {
        uint16_t local_code = (code >> (4 * pos)) & uint16_t(0x000f);
        stream.put(hex_codes[local_code]);
    }
}

static void utf16_create_surrogates(char32_t codepoint, uint16_t* high, uint16_t* low)
{
    // surrogate pair generation is slightly insane
    //   0000 0000 0000 nnnn  nnnn nnnn nnnn nnnn
    // - 0000 0000 0000 0001  0000 0000 0000 0000
    // ==========================================
    //   1101 10aa aaaa aaaa  1101 11bb bbbb bbbb
    // | high               | low                |
    uint32_t val = codepoint - 0x10000;
    *high = uint16_t(val >> 10)    | 0xd800;
    *low  = uint16_t(val & 0x03ff) | 0xdc00;
}

std::ostream& string_encode(std::ostream& stream, std::string_view source, bool ensure_ascii)
{
    typedef std::string_view::size_type size_type;

    for (size_type idx = 0, source_size = source.size(); idx < source_size; /* incremented inline */)
    {
        const char& current = source[idx];
        if (const char* replacement = find_encoding(current))
        {
            stream.put('\\').put(*replacement);
            ++idx;
        }
        else
        {
            unsigned length = utf8_sequence_length(&current, source.data() + source_size);
            const bool valid_utf8 = length != 0;

            if (!needs_unicode_escaping(current))
            {
                stream.put(current);
            }
            else
            {
                char32_t code;
                if (valid_utf8)
                {
                    code = utf8_decode(&current, length);
                }
                else
                {
                    // Invalid UTF-8 encoding -- we're either at the end of the string or the bytes were not a
                    // well-formed UTF-8 sequence. In either case, we will drop in a numeric encoding (\u00NN) for the
                    // byte and carry on from the next one.
                    length = 1;
                    code = char32_t(current) & 0xff;
                }

                // Without `ensure_ascii`, a well-formed multi-byte sequence passes through as it is. A single byte only
                // gets this far if it is an ASCII control or DEL, and those are escaped either way: JSON requires it
                // of the controls, and `utf8_strict` refuses DEL too.
                if (valid_utf8 && length > 1 && !ensure_ascii)
                {
                    stream.write(&current, length);
                }
                // basic multilingual plane points are encoded in hex
                else if (code < 0x10000)
                {
                    stream.write("\\u", 2);
                    to_hex(stream, uint16_t(code));
                }
                // Codepoints not in the basic multilingual plane must be encoded as surrogate pairs
                else
                {
                    uint16_t high, low;
                    utf16_create_surrogates(code, &high, &low);
                    stream.write("\\u", 2);
                    to_hex(stream, high);
                    stream.write("\\u", 2);
                    to_hex(stream, low);
                }
            }

            idx += length;
        }
    }

    return stream;
}

static uint16_t from_hex_digit(char c, std::size_t idx)
{
    switch (c)
    {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    // The JSON spec isn't clear if it wants capitals or lowercase, so I'll just assume both are okay
    case 'a':
    case 'A':
        return 0xa;
    case 'b':
    case 'B':
        return 0xb;
    case 'c':
    case 'C':
        return 0xc;
    case 'd':
    case 'D':
        return 0xd;
    case 'e':
    case 'E':
        return 0xe;
    case 'f':
    case 'F':
        return 0xf;
    default:
        throw decode_error(idx, std::string("The character '") + c + "' is not a valid hexidecimal digit.");
    }
}

static uint16_t from_hex(const char* s, std::size_t idx_base)
{
    uint16_t x = 0U;
    for (int idx = 3; idx >= 0; --idx)
    {
        x = uint16_t(x + (from_hex_digit(*s, idx_base + idx) << (idx * 4)));
        ++s;
    }

    return x;
}

static void utf8_sequence_info(char32_t val, std::size_t* length, char* first)
{
    if (val < 0x00000080U)
    {
        *length = 1;
        *first = char(val);
    }
    else if (val < 0x00000800U)
    {
        *length = 2;
        *first = char('\xc0' | ('\x1f' & (val >> 6)));
    }
    else if (val < 0x00010000U)
    {
        *length = 3;
        *first = char('\xe0' | ('\x0f' & (val >> 12)));
    }
    else
    {
        // Every caller hands over a `\u` escape or a combined surrogate pair, neither of which can reach past the
        // U+10FFFF ceiling, so the 5- and 6-byte forms RFC 3629 removed are never needed.
        assert(val <= 0x0010ffffU);
        *length = 4;
        *first = char('\xf0' | ('\x07' & (val >> 18)));
    }
}

static void utf8_append_code(std::string& str, char32_t val)
{
    char c;
    std::size_t length;
    utf8_sequence_info(val, &length, &c);

    char buffer[8];
    char* buffer_out = buffer;
    *buffer_out++ = c;

    std::size_t shift = (length - 2) * 6;
    for (std::size_t idx = 1; idx < length; ++idx)
    {
        c = char('\x80' | ('\x3f' & (val >> shift)));
        *buffer_out++ = c;
        shift -= 6;
    }

    str.append(buffer, buffer_out);
}

static bool utf16_combine_surrogates(uint16_t high, uint16_t low, char32_t* out)
{
    if ((high & 0xfc00U) != 0xd800 || (low & 0xfc00U) != 0xdc00)
    {
        // invalid surrogate pair
        return false;
    }
    else
    {
        // surrogate pairs take the form:
        // | high               | low                |
        //  1101 10aa aaaa aaaa  1101 11bb bbbb bbbb
        // result:
        //   0000 0000 0000 aaaa  aaaa aabb bbbb bbbb
        // + 0000 0000 0000 0001  0000 0000 0000 0000
        *out = 0x10000
             + (((char32_t(high) & 0x03ff) << 10) | (char32_t(low)  & 0x03ff));
        return true;
    }
}

/**
 *  \tparam require_printable Requires all characters in the sequence to be "printable" (aka: call \c std::isprint on
 *                            them). This will probably eventually eventually transform into a "strict mode."
**/
template <parse_options::encoding encoding, bool require_printable>
std::string string_decode(std::string_view source)
{
    typedef std::string::size_type size_type;

    std::string output;
    output.reserve(source.size()); // OPTIMIZATION: Reserve a more appropriate size
    const char* last_pushed_src = source.data();

    for (size_type idx = 0; idx < source.size(); /* incremented inline */)
    {
        const char& current = source[idx];
        if (current == '\\')
        {
            output.append(last_pushed_src, source.data()+idx);

            if (idx + 2 > source.size())
                throw decode_error(idx, "unterminated escape sequence (backslash at end of string)");
            const char& next = source[idx + 1];
            if (const char* replacement = find_decoding(next))
            {
                output += *replacement;
                idx += 2;
            }
            else if (next == 'u')
            {
                if (idx + 6 > source.size())
                    throw decode_error(idx, "unterminated Unicode escape sequence (must have 4 hex characters)");
                uint16_t hexval = from_hex(&source[idx + 2], idx + 2);

                if (hexval < 0xd800U || hexval > 0xdfffU)
                {
                    utf8_append_code(output, hexval);

                    idx += 6;
                }
                // numeric encoding is in U+d800 - U+dfff with UTF-8 output, so deal with surrogate pairing...
                else
                {
                    auto surrogateString = [&] () { return std::string(source.data()+idx, 6); };
                    if (  idx + 12 > source.size()
                       || idx +  8 > source.size()
                       || source[idx + 6] != '\\'
                       || source[idx + 7] != 'u'
                       )
                        throw decode_error(idx, std::string("unpaired high surrogate (") + surrogateString() + ")");
                    uint16_t hexlowval = from_hex(&source[idx + 8], idx + 8);
                    char32_t codepoint;
                    if (!utf16_combine_surrogates(hexval, hexlowval, &codepoint))
                        throw decode_error(idx, std::string("unpaired high surrogate (") + surrogateString() + ")");

                    utf8_append_code(output, codepoint);

                    idx += 12;
                }
            }
            else
            {
                throw decode_error(idx, std::string("Unknown escape character: ") + next);
                //output += '?'; Maybe better solution if we don't want to throw
                //++idx;
            }

            last_pushed_src = source.data() + idx;
        }
        else if (current & '\x80')
        {
            if (auto utf8_length = utf8_sequence_length(&current, source.data() + source.size()))
            {
                idx += utf8_length;
            }
            else JSONV_UNLIKELY
            {
                std::ostringstream os;
                os << "Invalid UTF-8 sequence beginning with \\x"
                   << std::hex << std::setfill('0') << std::setw(2) << unsigned(static_cast<unsigned char>(current));
                throw decode_error(idx, os.str());
            }
        }
        else if (require_printable && !is_print(current)) JSONV_UNLIKELY
        {
            std::ostringstream os;
            os << "Unprintable character found in input: ";
            switch (current)
            {
            case '\t': os << "\\t (tab)"; break;
            case '\b': os << "\\b (backspace)"; break;
            case '\f': os << "\\f (formfeed)"; break;
            case '\n': os << "\\n (newline)"; break;
            case '\r': os << "\\r (carriage return)"; break;
            default:   os << "\\x" << std::hex << std::setw(2) << static_cast<int>(current) << std::dec; break;
            }
            throw decode_error(idx, os.str());
        }
        else
        {
            ++idx;
        }
    }

    output.append(last_pushed_src, source.data() + source.size());
    return output;
}

string_decode_fn get_string_decoder(parse_options::encoding encoding)
{
    switch (encoding)
    {
    case parse_options::encoding::utf8_strict:
        return string_decode<parse_options::encoding::utf8, true>;
    case parse_options::encoding::utf8:
    default:
        return string_decode<parse_options::encoding::utf8, false>;
    };
}

/// The number of UTF-16 code units the UTF-8 \a source decodes into. This does not validate \a source -- the
/// conversion loop below remains the only validator, and it rejects everything this over-counts.
static std::size_t utf16_length_of_utf8(std::string_view source) noexcept
{
    std::size_t units = 0;

    for (std::size_t idx = 0; idx < source.size(); ++idx)
    {
        // NOTE(tgockel): `char` is signed here, so these comparisons must be made on an `unsigned char`. See issue
        // #108 for what signed `char` bit tests have cost us before.
        auto b = static_cast<unsigned char>(source[idx]);

        // Every byte which is not a sequence continuation starts a code point...
        if ((b & 0xc0U) != 0x80U)
            ++units;

        // ...and a code point outside the BMP needs a surrogate pair. Every sequence the conversion loop accepts
        // with a lead byte of 0xf0 or above decodes outside it, since anything shorter would be an overlong encoding.
        if (b >= 0xf0U)
            ++units;
    }

    return units;
}

std::wstring convert_to_wide(std::string_view source)
{
    auto expected_size = utf16_length_of_utf8(source);

    std::wstring out;
    out.reserve(expected_size);

    for (std::size_t source_idx = 0; source_idx < source.size(); /* inline */)
    {
        // The loop condition covers the first read and the `source_idx + steps` check below covers the rest, so an
        // unchecked subscript is always in range here.
        auto next_source = [&] () -> char32_t
                           {
                               assert(source_idx < source.size());
                               return static_cast<unsigned char>(source[source_idx++]);
                           };

        char32_t    codepoint;
        std::size_t steps;

        auto c = next_source();
        if (c <= 0x7f)
        {
            codepoint = c;
            steps     = 0;
        }
        else if (c <= 0xbf)
        {
            throw std::range_error("Invalid UTF-8: Invalid character");
        }
        else if (c <= 0xdf)
        {
            codepoint = c & 0x1f;
            steps     = 1;
        }
        else if (c <= 0xef)
        {
            codepoint = c & 0x0f;
            steps     = 2;
        }
        else if (c <= 0xf7)
        {
            codepoint = c & 0x07;
            steps     = 3;
        }
        else
        {
            throw std::range_error("Invalid UTF-8: Invalid character");
        }

        if (source_idx + steps > source.size())
            throw std::range_error("Invalid UTF-8: encoding sequence extends past end of source");

        for (std::size_t step = 0; step < steps; ++step)
        {
            auto in_c = next_source();
            if (in_c < 0x80 || in_c > 0xbf)
                throw std::range_error("Invalid UTF-8: invalid character");

            codepoint = (codepoint << 6) | (in_c & 0x3fU);
        }

        // The smallest code point each sequence length may encode -- anything below has a shorter encoding.
        static constexpr char32_t shortest_form_minimum[] = { 0x0U, 0x80U, 0x800U, 0x10000U };
        if (codepoint < shortest_form_minimum[steps])
            throw std::range_error("Invalid UTF-8: overlong encoding");

        if (codepoint >= 0xd800U && codepoint <= 0xdfffU)
            throw std::range_error("Invalid UTF-8: surrogate code point is not a Unicode character");

        if (codepoint > 0x10ffffU)
            throw std::range_error("Invalid UTF-8: code point is too large");

        if (codepoint <= 0xffffU)
        {
            out += wchar_t(codepoint);
        }
        else
        {
            uint16_t high, low;
            utf16_create_surrogates(codepoint, &high, &low);
            out += wchar_t(high);
            out += wchar_t(low);
        }
    }

    assert(out.size() == expected_size);
    return out;
}

/// The code unit \a unit holds, read at the full width of \c wchar_t. That is 32 bits on some platforms, and signed on
/// some, so a unit too wide for UTF-16 -- a negative one included -- comes out above 0xffff rather than truncated.
static char32_t utf16_code_unit(wchar_t unit) noexcept
{
    return static_cast<std::make_unsigned_t<wchar_t>>(unit);
}

/// The number of UTF-8 bytes the UTF-16 source encodes into. Like \c utf16_length_of_utf8, this is not a validator.
static std::size_t utf8_length_of_utf16(const wchar_t* source_data, std::size_t source_size) noexcept
{
    // NOTE(tgockel): Read through `utf16_code_unit` exactly as the conversion loop does. A unit above 0xffff can be
    // mistaken for a surrogate by the masks below, but the conversion loop rejects it, so miscounting it is fine.
    auto unit_at = [&] (std::size_t idx) { return utf16_code_unit(source_data[idx]); };

    std::size_t bytes = 0;

    for (std::size_t idx = 0; idx < source_size; /* inline */)
    {
        auto c = unit_at(idx++);

        // A high surrogate followed by a low one is a single code point needing 4 bytes. Everything else stands on
        // its own -- a lone low surrogate included, which the conversion loop rejects, so over-counting it is fine.
        if ((c & 0xfc00U) == 0xd800U && idx < source_size && (unit_at(idx) & 0xfc00U) == 0xdc00U)
        {
            bytes += 4U;
            ++idx;
        }
        else
        {
            bytes += (c <= 0x007fU) ? 1U
                   : (c <= 0x07ffU) ? 2U
                   :                  3U;
        }
    }

    return bytes;
}

static std::string convert_to_narrow(const wchar_t* source_data, std::size_t source_size)
{
    auto expected_size = utf8_length_of_utf16(source_data, source_size);

    std::string out;
    out.reserve(expected_size);

    for (std::size_t source_idx = 0; source_idx < source_size; /* inline */)
    {
        // A `std::wstring` is UTF-16 even where `wchar_t` is 32 bits. Narrowing a unit which does not fit would encode
        // some other character, and could turn an invalid surrogate pair into a valid one, so it is refused instead.
        // Both units of a pair are read through here, so this covers the low surrogate too.
        auto next_source = [&] () -> char32_t
                           {
                               auto unit = utf16_code_unit(source_data[source_idx++]);
                               if (unit > 0xffffU)
                                   throw std::range_error("Invalid UTF-16: code unit is wider than 16 bits");
                               return unit;
                           };

        char32_t codepoint;
        auto     c         = next_source();

        // A low surrogate with no high one in front of it. Encoding it on its own would produce the UTF-8 bytes of a
        // surrogate, which is not well-formed UTF-8 and which the parser would refuse to read back.
        if ((c & 0xfc00U) == 0xdc00U)
        {
            throw std::range_error("Invalid UTF-16: unpaired low surrogate");
        }
        // normal
        else if ((c & 0xfc00U) != 0xd800U)
        {
            codepoint = c;
        }
        // surrogate start
        else
        {
            // `next_source` has already stepped past the high surrogate, so `source_idx` is the index of the low
            // one -- there is no further code unit to require.
            if (source_idx >= source_size)
                throw std::range_error("Invalid UTF-16: surrogate extends past end of string");

            auto c_lo = next_source();
            if (!utf16_combine_surrogates(c, c_lo, &codepoint))
                throw std::range_error("Invalid UTF-16: invalid surrogate pair");
        }

        utf8_append_code(out, codepoint);
    }

    assert(out.size() == expected_size);
    return out;
}

std::string convert_to_narrow(const wchar_t* source)
{
    return convert_to_narrow(source, wcslen(source));
}

std::string convert_to_narrow(const std::wstring& source)
{
    return convert_to_narrow(source.c_str(), source.size());
}

}
}
