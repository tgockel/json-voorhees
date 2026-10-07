/// \file jsonv/writer.hpp
/// Write a JSON AST.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/forward.hpp>

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string_view>

namespace jsonv
{

/// \addtogroup Serialization
/// \{

/// A writer instance writes a JSON \ref ast_node sequence to some form of sink: an \c encoder, which turns the tokens
/// into text or into whatever else it builds.
///
/// This is the mirror of \c reader. A reader is a forward cursor over the tokens of a document somebody else wrote; a
/// writer is a forward cursor over the tokens of the document you are writing. Each token method writes one token and
/// returns \c *this, so calls chain. Writing an object means opening it, writing each member as a key followed by its
/// value, and closing it:
///
/// \code
/// struct my_object
/// {
///     std::int64_t             a = 0;
///     std::vector<std::string> tags;
/// };
///
/// void write_my_object(jsonv::writer& to, const my_object& from)
/// {
///     to.object_begin();
///     to.key("a").integer(from.a);
///     to.key("tags").array_begin();
///     for (const auto& tag : from.tags)
///         to.string(tag);
///     to.array_end();
///     to.object_end();
/// }
///
/// jsonv::ostream_pretty_encoder sink(std::cout);
/// jsonv::writer                 to(sink);
/// write_my_object(to, my_object{ 1, { "x", "y" } });
/// \endcode
///
/// The writer owns the grammar and the punctuation. It refuses a token the grammar does not allow where the cursor is
/// -- a key outside an object, a value in an object with no key before it, an end which does not match the open
/// structure -- with \c std::logic_error, before the sink sees anything; and it writes the delimiters between elements
/// and between members itself, so an \c encoder only ever sees a sequence of tokens which spells a valid document. A
/// writer at depth zero accepts another root value, which is how two documents end up in one stream.
///
/// \see encoder
/// \see reader
class JSONV_PUBLIC writer final
{
public:
    /// Create a writer which writes into \a to. The encoder must outlive this instance.
    explicit writer(encoder& to);

    /// Create a writer which writes compact JSON text into \a to, as \c to_string does for a \c value: through an
    /// \c ostream_encoder it owns, with \c ostream_encoder::ensure_ascii on. To pretty-print, or to write well-formed
    /// UTF-8 as it is, construct the encoder yourself and use the overload above. The stream must outlive this
    /// instance.
    explicit writer(std::ostream& to);

    // Not copyable.
    writer(const writer&)            = delete;
    writer& operator=(const writer&) = delete;

    /// \{

    /// Moving a writer transfers its state and its sink, leaving the source moved-from: \c good is \c false, \c depth
    /// is \c 0 and every other member throws \c std::invalid_argument. These are out-of-line because destroying the
    /// state needs a complete \c writer::impl, which this header does not have -- the same reason the destructor is.
    writer(writer&&) noexcept;
    writer& operator=(writer&&) noexcept;
    /// \}

    ~writer() noexcept;

    /// Check if this writer is still good to write to, which is to say that it has not been moved-from.
    JSONV_NODISCARD
    bool good() const noexcept;

    /// The number of structures -- objects and arrays -- currently open. This is \c 0 before the first token, between
    /// root values, and for a moved-from writer.
    JSONV_NODISCARD
    std::size_t depth() const noexcept;

    /// Get the path of the slot the next token fills.
    ///
    /// \code
    /// jsonv::writer to(sink);   /* "."     -- the root */
    /// to.object_begin();        /* "."     -- the object fills the root; the next token is a key, which has no slot */
    /// to.key("a");              /* ".a"    -- the value of "a" comes next */
    /// to.array_begin();         /* ".a[0]" -- the first element comes next */
    /// to.integer(1);            /* ".a[1]" */
    /// to.integer(2);            /* ".a[2]" */
    /// to.array_end();           /* "."     -- back in the object, where the next token is a key */
    /// to.key("b");              /* ".b"    */
    /// to.object_begin();        /* ".b"    */
    /// to.key("x");              /* ".b.x"  */
    /// to.string("taco");        /* ".b"    */
    /// to.object_end();          /* "."     */
    /// to.object_end();          /* "."     -- the document is complete; another root may follow */
    /// \endcode
    ///
    /// This is where a \c serializer is when it finds it cannot write the value it was asked for, which is what the
    /// path is for. It is built from the stack of open structures on demand and kept until the next token, so a
    /// document which never asks never pays for it -- unlike \c reader::current_path over text, which rescans the
    /// document. The convention differs from the reader's, which names the token it is *on*: a reader on an element
    /// and a writer about to write that element agree, as do the two at a key, while a reader on a closing token names
    /// the structure it closes and a writer which has just closed one names where its next token goes.
    ///
    /// \throws std::invalid_argument if this instance has been moved-from.
    JSONV_NODISCARD
    const path& current_path() const;

    /// \{

    /// Write the token which opens or closes an object or an array.
    ///
    /// \code
    /// {
    /// \endcode
    ///
    /// Opening one is writing a value, so it is allowed wherever a value is: at depth zero, as an array element, or as
    /// the value of the \c key just written. Closing one must match the innermost open structure, and an object cannot
    /// close while a key is waiting for its value.
    ///
    /// \throws std::logic_error if the grammar does not allow the token here. Nothing has reached the sink.
    /// \throws std::invalid_argument if this instance has been moved-from.
    writer& object_begin();
    writer& object_end();
    writer& array_begin();
    writer& array_end();
    /// \}

    /// Write the \a key of the next member of the open object, including the separator.
    ///
    /// \code
    /// "key":
    /// \endcode
    ///
    /// The value must follow before the next key or the end of the object. \a key is viewed, not kept: it has to live
    /// until this returns and no longer.
    ///
    /// \throws std::logic_error if no object is open, if the innermost open structure is an array, or if the key before
    ///                          this one is still waiting for its value. Nothing has reached the sink.
    /// \throws std::invalid_argument if this instance has been moved-from.
    writer& key(std::string_view key);

    /// \{

    /// Write a scalar value.
    ///
    /// \code
    /// null
    /// true
    /// 902
    /// 4.9
    /// "value"
    /// \endcode
    ///
    /// A value is allowed at depth zero, as an array element, or as the value of the \c key just written. What a
    /// \c decimal with no JSON representation -- a NaN or an infinity -- becomes is the encoder's choice, as is what
    /// happens to a \c string which is not valid UTF-8 (see \c encoder::write_decimal and \c encoder::write_string).
    ///
    /// \throws std::logic_error if an object is open and no key is waiting for its value. Nothing has reached the sink.
    /// \throws std::invalid_argument if this instance has been moved-from.
    writer& null();
    writer& boolean(bool value);
    writer& integer(std::int64_t value);
    writer& decimal(double value);
    writer& string(std::string_view value);
    /// \}

    /// Write the whole of \a source as one value: a tree walk which writes every node in order, with an object's
    /// members in the order the object keeps them, which is sorted by key. This is what \c encoder::encode does.
    ///
    /// \throws std::logic_error if a value is not allowed here, as for the scalar functions. Nothing has reached the
    ///                          sink.
    /// \throws std::invalid_argument if this instance has been moved-from.
    writer& write(const value& source);

    /// Write the whole of \a source as one value, handing it over to the sink. An \c encoder which builds a tree takes
    /// it as it is rather than rebuilding it node by node, and one producing text walks it exactly as the overload
    /// above does. \a source is left moved-from.
    ///
    /// \throws std::logic_error if a value is not allowed here, as for the scalar functions. Nothing has reached the
    ///                          sink and \a source is untouched.
    /// \throws std::invalid_argument if this instance has been moved-from.
    writer& write(value&& source);

private:
    class impl;

private:
    std::unique_ptr<impl> _impl;
};

/// \}

}
