/** \file
 *  
 *  Copyright (c) 2012 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include "char_convert.hpp"

#include "detail/fixed_map.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <map>
#include <stdexcept>

namespace jsonv
{
namespace detail
{

decode_error::decode_error(size_type offset, const std::string& message):
        runtime_error(message),
        _offset(offset)
{ }

decode_error::~decode_error() throw()
{ }

#define ESCAPES_LIST(item) \
    item('\b', 'b')  \
    item('\f', 'f')  \
    item('\n', 'n')  \
    item('\r', 'r')  \
    item('\t', 't')  \
    item('\\', '\\') \
    item('/',  '/')  \
    item('\"', '\"') \

#define TUPLE_PLUS_1_GEN(a, b) +1
typedef detail::fixed_map<char_type, char_type, ESCAPES_LIST(TUPLE_PLUS_1_GEN)> converter_map;

/** These entries are sorted by the numeric value of the ASCII character (\c less_entry_cpp).
 *  
 *  \note
 *  The encode and decode map must be in a different order (even though they contain the same data) because the ASCII
 *  representations of escape sequences are not in the same order as the characters they are escaping.
**/
#define TUPLE_FIRST_SECOND(a, b) { a, b },
const converter_map encode_map = { ESCAPES_LIST(TUPLE_FIRST_SECOND) };

/** These entries are sorted by the character value of the escape sequence (\c less_entry_json).
**/
#define TUPLE_SECOND_FIRST(a, b) { b, a },
const converter_map decode_map = { ESCAPES_LIST(TUPLE_SECOND_FIRST) };

const char_type* find(const converter_map& source, char_type key)
{
    converter_map::const_iterator iter = source.find(key);
    if (iter != source.end())
        return &iter->second;
    else
        return NULL;
}

static const char_type* find_encoding(char_type native_character)
{
    return find(encode_map, native_character);
}

static const char_type* find_decoding(char_type char_after_backslash)
{
    return find(decode_map, char_after_backslash);
}

static bool needs_unicode_escaping(char c)
{
    return !isprint(c)
        || (c & '\x80');
}

static bool char_bitmatch(char c, char pos, char neg)
{
    return (c & pos) == pos
        && !(c & neg);
}

static void utf8_extract_info(char c, unsigned& length, char& bitmask)
{
    if (!(c & '\x80'))
    {
        length = 1;
        bitmask = '\x7f';
    }
    else if (char_bitmatch(c, '\xc0', '\x20'))
    {
        length = 2;
        bitmask = '\x1f';
    }
    else if (char_bitmatch(c, '\xe0', '\x10'))
    {
        length = 3;
        bitmask = '\x0f';
    }
    else if (char_bitmatch(c, '\xf0', '\x08'))
    {
        length = 4;
        bitmask = '\x07';
    }
    else if (char_bitmatch(c, '\xf8', '\x04'))
    {
        length = 5;
        bitmask = '\x03';
    }
    else if (char_bitmatch(c, '\xfc', '\x02'))
    {
        length = 6;
        bitmask = '\x01';
    }
    else
    {
        // This is not an acceptable/valid UTF8 string.  A failure here means I can't trust or don't understand the
        // encoding on the platform.
        assert(false);
        // In release mode, we'll spit out *potentially* garbage JSON.
        length = 1;
        bitmask = '\x7f';
    }
}

/** All these code extraction algorithms are probably wrong...the UTF8 encoding standard allows for 6-byte sequences,
 *  which allows for all 2^31 UCS codes. However, Unicode ends at 0x10ffff with the PUA-B plane (which people shouldn't
 *  be sending around in JSON in the first place). JSON's escape sequence for unicode is 4 hexidecimal digits, which is
 *  only a \c uint16_t. This means that JSON is only capable of encoding the basic multilingual plane, which means no
 *  alchemical symbols ಠಠ.
**/
static uint16_t utf8_extract_code(const char* c, unsigned length, char bitmask)
{
    const char submask = '\x3f';
    
    assert(length <= 4);
    uint16_t num(*c & bitmask);
    ++c;
    
    for (unsigned i = 1; i < length; ++i, ++c)
    {
        assert(char_bitmatch(*c, '\x80', '\x40'));
        num = uint16_t(num << 6);
        num = uint16_t(num | (*c & submask));
    }
    
    return num;
}

static const char hex_codes[] = "0123456789abcdef";

static void to_hex(ostream_type& stream, uint16_t code)
{
    for (int pos = 3; pos >= 0; --pos)
    {
        uint16_t local_code = (code >> (4 * pos)) & uint16_t(0x000f);
        stream << hex_codes[local_code];
    }
}

ostream_type& string_encode(ostream_type& stream, const string_type& source)
{
    typedef string_type::size_type size_type;
    
    for (size_type idx = 0, source_size = source.size(); idx < source_size; /* incremented inline */)
    {
        const char_type& current = source[idx];
        if (const char_type* replacement = find_encoding(current))
        {
            stream << "\\" << *replacement;
            ++idx;
        }
        else
        {
            unsigned length;
            char bitmask;
            utf8_extract_info(current, length, bitmask);
            assert(idx + length <= source_size);
            
            if (!needs_unicode_escaping(current))
                stream << current;
            else
            {
                uint16_t code = utf8_extract_code(&current, length, bitmask);
                stream << "\\u";
                to_hex(stream, code);
            }
            
            idx += length;
        }
    }
    
    return stream;
}

static uint16_t from_hex_digit(char c)
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
        throw std::range_error(std::string("The character '") + c + "' is not a valid hexidecimal digit.");
    }
}

static uint16_t from_hex(const char* s)
{
    uint16_t x = 0U;
    for (int idx = 3; idx >= 0; --idx)
    {
        x = uint16_t(x + (from_hex_digit(*s) << (idx * 4)));
        ++s;
    }
    
    return x;
}

static void utf8_sequence_info(uint16_t val, unsigned& length, char& first)
{
    if (val < 0x00000080U)
    {
        length = 1;
        first = char(val);
    }
    else if (val < 0x00000800U)
    {
        length = 2;
        first = char('\xc0' | ('\x1f' & (val >> 6)));
    }
    else// if (val < 0x00010000U)
    {
        length = 3;
        first = char('\xe0' | ('\x0f' & (val >> 12)));
    }
    /*
    else if (val < 0x00200000U)
    {
        length = 4;
        first = char('\xf0' | ('\x07' & (val >> 18)));
    }
    else if (val < 0x04000000U)
    {
        length = 5;
        first = char('\xf8' | ('\x03' & (val >> 24)));
    }
    else
    {
        length = 6;
        first = char('\xfc' | char('\x01' & (val >> 30)));
    }
    */
}

static void utf8_append_code(string_type& str, uint16_t val)
{
    char c;
    unsigned length;
    utf8_sequence_info(val, length, c);
    
    // HACK: This function is crap.
    str += c;
    int shift = (length - 2) * 6;
    for (unsigned idx = 1; idx < length; ++idx)
    {
        c = char('\x80' | ('\x3f' & (val >> shift)));
        str += c;
        shift -= 6;
    }
}

string_type string_decode(const string_type& source)
{
    typedef string_type::size_type size_type;
    
    string_type output;
    
    for (size_type idx = 0, source_size = source.size(); idx < source_size; /* incremented inline */)
    {
        const char_type& current = source[idx];
        if (current == '\\')
        {
            const char_type& next = source[idx + 1];
            if (const char_type* replacement = find_decoding(next))
            {
                output += *replacement;
                idx += 2;
            }
            else if (next == 'u')
            {
                if (idx + 6 > source_size)
                    throw decode_error(idx, "unterminated Unicode escape sequence (must have 4 hex characters)");
                uint16_t hexval = from_hex(&source[idx + 2]);
                utf8_append_code(output, hexval);
                
                idx += 6;
            }
            /* TODO: 8-character Unicode escape support...not technically part of the standard, but I don't see why it
             *       isn't...embrace extend extinguish?
             * 
             * else if (next == 'U')
             * {
             *     if (idx + 10 > source_size)
             *         throw decode_error(idx, "unterminated Unicode escape sequence (must have 4 hex characters)");
             *      uint32_t hexval = from_hex(&source[idx + 2], 8);
             *      utf8_append_code(output, hexval);
             *   
             *      idx += 10;
             * }
             */
            else
            {
                throw decode_error(idx, std::string("Unknown escape character: ") + next);
                //output += '?'; Maybe better solution if we don't want to throw
                //++idx;
            }
        }
        else
        {
            output += current;
            ++idx;
        }
    }
    
    return output;
}

}
}
