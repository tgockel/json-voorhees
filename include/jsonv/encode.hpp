/** \file jsonv/encode.hpp
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
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/forward.hpp>
#include <string_view>

#include <cstdint>
#include <iosfwd>
#include <memory>

namespace jsonv
{

/** An encoder is responsible for writing values to some form of output.
 *
 *  It is a sink for the tokens of a JSON document: a \c writer drives the \c write_ hooks below one token at a time,
 *  writing the delimiters between elements and members itself. A whole \c value, handed to \c encode or to
 *  \c writer::write, arrives through \c write_tree, which by default walks it through those same hooks. An
 *  implementation turns each token into text, as \c ostream_encoder does, or into anything else.
 *
 *  \see writer
**/
class JSONV_PUBLIC encoder
{
public:
    virtual ~encoder() noexcept;

    /** Encode some source value into this encoder, through \c write_tree. To write a document token by token instead
     *  of from a finished \c value, construct a \c writer over this encoder; its \c writer::write does the same.
    **/
    void encode(const jsonv::value& source);

protected:
    /** Write the null value.
     *  
     *  \code
     *  null
     *  \endcode
    **/
    virtual void write_null() = 0;
    
    /** Write the opening of an object value.
     *  
     *  \code
     *  {
     *  \endcode
    **/
    virtual void write_object_begin() = 0;
    
    /** Write the closing of an object value.
     *  
     *  \code
     *  }
     *  \endcode
    **/
    virtual void write_object_end() = 0;
    
    /** Write the key for an object, including the separator.
     *  
     *  \code
     *  "key":
     *  \endcode
    **/
    virtual void write_object_key(std::string_view key) = 0;
    
    /** Write the delimiter between two entries in an object.
     *  
     *  \code
     *  ,
     *  \endcode
    **/
    virtual void write_object_delimiter() = 0;
    
    /** Write the opening of an array value.
     *  
     *  \code
     *  [
     *  \endcode
    **/
    virtual void write_array_begin() = 0;
    
    /** Write the closing of an array value.
     *  
     *  \code
     *  ]
     *  \endcode
    **/
    virtual void write_array_end() = 0;
    
    /** Write the delimiter between two entries in an array.
     *  
     *  \code
     *  ,
     *  \endcode
    **/
    virtual void write_array_delimiter() = 0;
    
    /** Write a string value.
     *  
     *  \param value is the string to write. It will \e hopefully be encoded as valid UTF-8. It is the implementation's
     *               choice of how to deal with malformed string values. Two common options are to replace malformed
     *               sequences with ?s or to simply output these encodings and let the receiver deal with them.
     *  
     *  \code
     *  "value"
     *  \endcode
    **/
    virtual void write_string(std::string_view value) = 0;
    
    /** Write an integer value.
     *  
     *  \code
     *  902
     *  \endcode
    **/
    virtual void write_integer(std::int64_t value) = 0;
    
    /** Write a decimal value.
     *  
     *  \param value is the decimal to write. Keep in mind that standard JSON does not support special IEEE 754 values
     *               such as NaN and infinity. It is the implementation's choice of how to deal with such values. Two
     *               common options are to output \c null or to encode a string description of the special value.
     *  
     *  \code
     *  4.9
     *  \endcode
    **/
    virtual void write_decimal(double value) = 0;
    
    /** Write a boolean value.
     *
     *  \code
     *  true
     *  \endcode
    **/
    virtual void write_boolean(bool value) = 0;

    /** Write \a source and everything under it as one value, taking it over. A \c writer calls this for a \c value it
     *  is handed as an rvalue. The default writes it as an lvalue is written, through the overload below; a sink
     *  which builds a tree, as \c value_encoder does, overrides it to take the value as it is rather than copy it.
    **/
    virtual void write_tree(jsonv::value&& source);

    /** Write \a source and everything under it as one value. \c encode calls this, and so does a \c writer for a
     *  \c value it is handed as an lvalue. The default walks the tree through the hooks above, with an object's
     *  members in the order the object keeps them and the delimiters written here, which is what a sink producing
     *  text wants: a \c value is well-formed by construction, so nothing checks the grammar inside one. A sink which
     *  builds a tree, as \c value_encoder does, overrides it to copy the value whole rather than rebuild it node by
     *  node.
    **/
    virtual void write_tree(const jsonv::value& source);

private:
    /** The walk behind the default \c write_tree. It recurses into itself rather than into the hook, which is for a
     *  whole value handed in and not for each subtree of one.
    **/
    void walk_tree(const jsonv::value& source);

    /// The hooks above are driven by a \c writer, which is what keeps the sequence of calls spelling a valid document.
    friend class writer;
};

/** An encoder that outputs to an \c std::ostream. This implementation is used for \c operator<< on a \c value.
 *
 *  Text goes into the stream unformatted, so the stream's format flags, width, fill and locale have no say in the JSON
 *  written, and are left as the caller set them. A width set before <tt>os << some_value</tt> applies to whatever is
 *  inserted after it, not to the document.
**/
class JSONV_PUBLIC ostream_encoder :
        public encoder
{
public:
    /** Create an instance which places text into \a output. **/
    explicit ostream_encoder(std::ostream& output);
    
    virtual ~ostream_encoder() noexcept;
    
    /** If set to true (the default), then all non-ASCII characters in strings will be replaced with their numeric
     *  encodings. Since JSON allows for encoded text to be contained in a document, this is inefficient if you have
     *  many non-ASCII characters. If you know that your decoding side can properly handle UTF-8 encoding, then you
     *  should turn this off, and well-formed UTF-8 will be written out as it is. An \c ostream_pretty_encoder follows
     *  this setting too.
     *
     *  \note
     *  This functionality cannot be used to passthrough malformed UTF-8 encoded strings or control characters. If a
     *  given string is invalid UTF-8, it will still get replaced with a numeric encoding, and so will any character
     *  JSON requires to be escaped.
    **/
    void ensure_ascii(bool value);
    
protected:
    virtual void write_null() override;
    
    virtual void write_object_begin() override;
    
    virtual void write_object_end() override;
    
    virtual void write_object_key(std::string_view key) override;
    
    virtual void write_object_delimiter() override;
    
    virtual void write_array_begin() override;
    
    virtual void write_array_end() override;
    
    virtual void write_array_delimiter() override;
    
    virtual void write_string(std::string_view value) override;
    
    virtual void write_integer(std::int64_t value) override;
    
    /** When a special value is given, this will output \c null. **/
    virtual void write_decimal(double value) override;
    
    virtual void write_boolean(bool value) override;
    
protected:
    /** The stream text goes into. Write to it unformatted, with \c put and \c write, as this class does. A formatted
     *  insertion would let the stream's state into the JSON, and would use up a width the caller left pending.
    **/
    std::ostream& output();
    
private:
    std::ostream& _output;
    bool          _ensure_ascii;
};

/** Like \c ostream_encoder, but pretty prints output to an \c std::ostream. For example, to pretty-print JSON to
 *  \c std::cout:
 *  
 *  \code
 *  jsonv::ostream_pretty_encoder encoder(std::cout);
 *  encoder.encode(some_value);
 *  encoder.encode(another_value);
 *  \endcode
**/
class JSONV_PUBLIC ostream_pretty_encoder :
        public ostream_encoder
{
public:
    /** Create an instance which places text into \a output. **/
    explicit ostream_pretty_encoder(std::ostream& output, std::size_t indent_size = 2);
    
    virtual ~ostream_pretty_encoder() noexcept;
    
protected:
    virtual void write_null() override;
    
    virtual void write_object_begin() override;
    
    virtual void write_object_end() override;
    
    virtual void write_object_key(std::string_view key) override;
    
    virtual void write_object_delimiter() override;
    
    virtual void write_array_begin() override;
    
    virtual void write_array_end() override;
    
    virtual void write_array_delimiter() override;
    
    virtual void write_string(std::string_view value) override;
    
    virtual void write_integer(std::int64_t value) override;
    
    virtual void write_decimal(double value) override;
    
    virtual void write_boolean(bool value) override;
    
private:
    void write_prefix();
    
    void write_eol();
    
private:
    std::size_t   _indent;
    std::size_t   _indent_size;
    bool          _defer_indent;
};

/** An encoder which builds a \c value from the tokens it is given: the sink for a document which is produced token by
 *  token through a \c writer but is wanted as a tree. It is the mirror of \c reader::from_value, which hands a tree
 *  out as tokens.
 *
 *  \code
 *  jsonv::value_encoder sink;
 *  jsonv::writer        to(sink);
 *  to.object_begin()
 *      .key("a").integer(1)
 *      .key("b").array_begin().string("x").string("y").array_end()
 *    .object_end();
 *  jsonv::value built = std::move(sink).take();   // {"a":1,"b":["x","y"]}
 *  \endcode
 *
 *  An object is built as a \c value keeps one, so a key which repeats within it keeps the value written last, as
 *  \c parse keeps it by default; a document built from tokens and the same document parsed from text select the same
 *  data. The encoder holds one document at a time: a second root value written before \c take replaces the first, as
 *  a repeated key does.
 *
 *  \see writer
 *  \see reader::from_value
**/
class JSONV_PUBLIC value_encoder final :
        public encoder
{
public:
    /** Create an instance holding nothing. **/
    explicit value_encoder();

    // Not copyable or movable. A writer keeps a pointer to its encoder, so an encoder which moved out from under it
    // would be a bug rather than a feature, and nothing else needs one to move.
    value_encoder(const value_encoder&)            = delete;
    value_encoder& operator=(const value_encoder&) = delete;
    value_encoder(value_encoder&&)                 = delete;
    value_encoder& operator=(value_encoder&&)      = delete;

    virtual ~value_encoder() noexcept override;

    /** Hand out the document written so far. This instance is left holding nothing, as it was constructed, so another
     *  document may follow.
     *
     *  \throws std::logic_error if an object or array is still open, or if nothing has been written at all. A JSON
     *                           document is never empty, so a writer which has produced nothing has not produced
     *                           \c null.
    **/
    JSONV_NODISCARD
    value take() &&;

protected:
    virtual void write_null() override;

    virtual void write_object_begin() override;

    virtual void write_object_end() override;

    /** Keep \a key for the member whose value comes next. **/
    virtual void write_object_key(std::string_view key) override;

    /** Does nothing: a tree has no punctuation. **/
    virtual void write_object_delimiter() override;

    virtual void write_array_begin() override;

    virtual void write_array_end() override;

    /** Does nothing: a tree has no punctuation. **/
    virtual void write_array_delimiter() override;

    /** The string is copied into the tree as it is, valid UTF-8 or not. **/
    virtual void write_string(std::string_view value) override;

    virtual void write_integer(std::int64_t value) override;

    /** Kept as given: a \c value can hold a NaN or an infinity, so nothing is substituted for one. **/
    virtual void write_decimal(double value) override;

    virtual void write_boolean(bool value) override;

    /** Takes \a source as it is. A tree handed over whole is placed where the next value goes without being rebuilt,
     *  which is what keeps a serializer on the \c value bridge linear in the depth of what it serializes.
    **/
    virtual void write_tree(jsonv::value&& source) override;

    /** Copies \a source in whole. A tree written as an lvalue -- \c to_json of a \c value, or of anything holding
     *  one -- costs what copying it does rather than a rebuild through the hooks above, which inserts each member and
     *  grows each array one element at a time.
    **/
    virtual void write_tree(const jsonv::value& source) override;

private:
    class impl;

private:
    std::unique_ptr<impl> _impl;
};

}
