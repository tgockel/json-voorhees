/// \file jsonv/reader.hpp
/// Read a JSON AST.
///
/// Copyright (c) 2015-2022 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/ast.hpp>
#include <jsonv/optional.hpp>
#include <expected>
#include <string_view>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iosfwd>
#include <iterator>
#include <memory>
#include <string>

namespace jsonv
{

class parse_index;
class parse_options;
class path;
class value;

namespace detail
{

class reader_lookahead;

}

/// \addtogroup Serialization
/// \{

/// A reader instance reads from some form of JSON source (probably a string) and converts it into a JSON \ref ast_node
/// sequence.
///
/// Readers normalize access to JSON source for conversion to some other format. They can be provided with pre-parsed
/// JSON through a \c parse_index or \c value. They can be provided with a \c std::string or \c std::string_view
/// directly. This allows \c deserializer implementations to operate on all forms of JSON without worrying about the
/// implementation.
///
/// A reader is a forward cursor over that sequence. It starts on \c ast_node_type::document_start, so the first thing
/// to do is step onto the value itself -- which \c jsonv::deserialize does for you when handed a fresh reader. Reading
/// an object means walking its keys, handling the ones you recognize and skipping the ones you do not:
///
/// \code
/// struct my_object
/// {
///     std::int64_t a = 0;
/// };
///
/// std::optional<my_object> deserialize_my_object(jsonv::reader& from)
/// {
///     if (!from.expect(jsonv::ast_node_type::object_begin))
///         return std::nullopt;
///
///     // Step onto the first key, or onto the } of an empty object.
///     if (!from.next_token())
///         return std::nullopt;
///
///     my_object out;
///     while (from.good() && from.current_type() != jsonv::ast_node_type::object_end)
///     {
///         // Keys arrive canonical or escaped, depending on whether the source used escape sequences.
///         if (!from.expect({ jsonv::ast_node_type::key_canonical, jsonv::ast_node_type::key_escaped }))
///             return std::nullopt;
///
///         auto key = from.current().visit_key([](const auto& k) { return std::string(k.value()); });
///         if (key == "a")
///         {
///             if (!from.next_token())
///                 return std::nullopt;
///
///             if (auto node = from.current_as<jsonv::ast_node::integer>())
///                 out.a = node->value();
///             else
///                 return std::nullopt;
///
///             // Step off the value and onto the next key, or onto the closing }.
///             if (!from.next_token())
///                 return std::nullopt;
///         }
///         else
///         {
///             // A key we do not care about -- skip its value, however large, and land on the next key.
///             if (!from.next_key())
///                 return std::nullopt;
///         }
///     }
///     return out;
/// }
/// \endcode
///
/// Note that \c next_key is only valid while sitting on a key or on the opening \c { -- it is the "skip this member"
/// step, not the loop's advance. Use \c next_token to move off a value you have just read.
class JSONV_PUBLIC reader final
{
public:
    /// Create a reader which reads from the given \a index.
    explicit reader(parse_index index);

    /// \{

    /// Create a reader which reads from an in-memory \a value.
    ///
    /// \param value The value to read from. The overload taking a reference does not copy it, so it must remain
    ///              valid for the lifetime of the reader; the rvalue overload moves \a value into the reader, which
    ///              then keeps it alive.
    ///
    /// These are named rather than constructors because \c value converts implicitly from \c std::string, among
    /// others. A \c reader(const value&) constructor would make <tt>reader(some_std_string)</tt> ambiguous -- a
    /// \c std::string reaches \c std::string_view and \c value through user-defined conversions of equal rank --
    /// and the same trap would reopen for every type \c value grows a converting constructor from. Returning by
    /// value costs nothing: the result is a prvalue of the returned type, so it initializes the caller's object
    /// directly without a move.
    JSONV_NODISCARD
    static reader from_value(const value& value);
    JSONV_NODISCARD
    static reader from_value(value&& value);
    /// \}

    /// \{

    /// Create a reader which reads from JSON \a source.
    ///
    /// The \a source must stay in memory for the duration of this instance's use, unless it is an rvalue reference to
    /// a \c std::string, which is moved to the reader's implementation to keep it alive. It is parsed with
    /// \a parse_options where they are given, and with the defaults \c parse_index::parse uses where they are not.
    explicit reader(std::string_view source);
    explicit reader(std::string_view source, const parse_options& parse_options);
    explicit reader(const char* source);
    explicit reader(const char* source, const parse_options& parse_options);

    explicit reader(std::string&& source);
    explicit reader(std::string&& source, const parse_options& parse_options);
    /// \}

    /// \{

    /// Create a reader which reads JSON from what is left of the \a source stream, as \c parse(std::istream&) does.
    ///
    /// \code
    /// std::ifstream file("file.json");
    /// jsonv::reader from(file);
    /// \endcode
    ///
    /// The stream is read to its end here, and the reader keeps the text, so \c owns_source is \c true and nothing
    /// reads \a source again once this returns. The text is parsed with \a parse_options where they are given, and
    /// with the defaults \c parse_index::parse uses where they are not. A \c parse_error position counts from where
    /// \a source was when this was called.
    explicit reader(std::istream& source);
    explicit reader(std::istream& source, const parse_options& parse_options);
    /// \}

    // Not copyable.
    reader(const reader&)            = delete;
    reader& operator=(const reader&) = delete;

    /// \{

    /// Moving a reader transfers its implementation, leaving the source moved-from: \c good is \c false and the
    /// accessors throw \c std::invalid_argument. These are out-of-line because destroying the implementation needs a
    /// complete \c reader::impl, which this header does not have -- the same reason the destructor is.
    reader(reader&&) noexcept;
    reader& operator=(reader&&) noexcept;
    /// \}

    ~reader() noexcept;

    /// Check if this reader is still good to read from. This will be \c true if this instance has not been moved-from
    /// and has not reached EOF. If this is \c false, \c current or \c current_path will throw an exception.
    JSONV_NODISCARD
    bool good() const;

    /// Check that the source this reader was created from is valid JSON.
    ///
    /// Parsing text never throws: a malformed document produces a sequence which stops at an \c ast_node_type::error
    /// node, and a reader walks it as far as it goes. What was wrong is not on that node in any form a reader of it can
    /// act on, so this is the question \c parse_index::validate answers, asked of the reader. It is about the source,
    /// not the cursor, so the answer is the same wherever the reader is positioned.
    ///
    /// \throws parse_error if this reader is over JSON text which did not parse. A reader over a \c value has nothing
    ///                     to parse and never throws this.
    /// \throws std::invalid_argument if this instance has been moved-from.
    void validate() const;

    /// Does this reader own the storage it reads from?
    ///
    /// This is \c true for a reader created from a \c std::string rvalue or by \c from_value(value&&), which keep their
    /// source alive for exactly as long as the reader. Anything deserialized as a view of the source -- a
    /// \c std::string_view, say -- is then valid only while the reader is. It is \c false for every other source,
    /// where the caller owns the storage, and for a moved-from reader.
    JSONV_NODISCARD
    bool owns_source() const noexcept;

    /// Get the current AST node this reader is pointing at.
    ///
    /// \throws std::logic_error if this instance is not \c good, or std::invalid_argument if it has been moved-from.
    JSONV_NODISCARD
    const ast_node& current() const;

    /// Get the type of the \c current AST node, which is always \c current().type().
    ///
    /// Use this where the type is all that is wanted -- checking for the `]` which ends an array, say. A reader over a
    /// \c value has to synthesise token text for a number, a string or a key before it can hand out an \c ast_node,
    /// and answering this does not.
    ///
    /// \throws std::logic_error if this instance is not \c good, or std::invalid_argument if it has been moved-from.
    JSONV_NODISCARD
    ast_node_type current_type() const;

    /// \{

    /// Check that the \c current AST node has the given \a type or is one of the expected \a types.
    ///
    /// \returns Nothing if the \c current node matches \a type or one of the given \a types; otherwise the
    ///          \c ast_node_type the \c current node actually has.
    /// \throws std::invalid_argument if \a types is empty.
    /// \throws std::logic_error if this instance is not \c good, or std::invalid_argument if it has been moved-from.
    ///
    /// \see ast_node::expect
    JSONV_NODISCARD
    std::expected<void, ast_node_type> expect(ast_node_type type) const;
    JSONV_NODISCARD
    std::expected<void, ast_node_type> expect(std::initializer_list<ast_node_type> types) const;
    /// \}

    /// Get the \c current AST node as a specific \c TAstNode subtype, calling \c expect beforehand.
    ///
    /// \returns The \c current node as a \c TAstNode; otherwise the \c ast_node_type the \c current node actually has.
    /// \throws std::logic_error if this instance is not \c good, or std::invalid_argument if it has been moved-from.
    template <typename TAstNode>
    JSONV_NODISCARD
    std::expected<TAstNode, ast_node_type> current_as() const
    {
        // Written as an explicit branch rather than `expect(...).transform(...)` on purpose. The monadic operations on
        // `std::expected` are a later addition than the type itself -- libstdc++ 12 and libc++ 16 have `expected` but
        // no `transform` -- so using one here would quietly raise the minimum toolchain by a whole release.
        if (auto matched = expect(TAstNode::type()); !matched)
            return std::unexpected(matched.error());
        else
            return current().as<TAstNode>();
    }

    /// Get the in-memory \c value this reader is positioned on, if it has one to lend.
    ///
    /// A reader created by \c from_value is walking a \c value which already exists, so the subtree under \c current
    /// is something it can hand out by reference rather than rebuild. A reader over JSON text has no such tree.
    ///
    /// Deserialization uses this to avoid copying a subtree it was already given, and so that a deserializer which
    /// returns a view of what it was handed -- \c std::string_view among them -- borrows the caller's storage rather
    /// than a temporary which dies with the call.
    ///
    /// \returns The value \c current names; or nothing if this reader is not value-backed, is not \c good, or is
    ///          positioned somewhere which does not start a value, such as an object key or a closing token.
    JSONV_NODISCARD
    optional<const value&> current_value() const noexcept;

    /// Get the path to the current node this reader is pointing at. This is used in the generation of error messages to
    /// describe the location of something that could not be deserialized.
    ///
    /// \code
    /// ^               /* "."  -- start of document is the empty path */
    /// {               /* "."  -- opening { is still an empty path */
    ///   "a":          /* ".a" -- the key starts the path */
    ///     [           /* ".a" -- the path refers to the entire array */
    ///       1,        /* ".a[0]" */
    ///       2,        /* ".a[1]" */
    ///       3,        /* ".a[2]" */
    ///     ],          /* ".a" -- the path at the end of the array refers to the entire array again */
    ///   "b":          /* ".b" */
    ///     {           /* ".b" -- the path refers to the entire object */
    ///       "x":      /* ".b.x" */
    ///         "taco"  /* ".b.x" */
    ///     },          /* ".b" */
    ///   "c":          /* ".c" */
    ///     4           /* ".c" */
    /// }               /* "." */
    /// $               /* "." */
    /// \endcode
    ///
    /// \throws std::logic_error if this instance is not \c good, or std::invalid_argument if it has been moved-from.
    JSONV_NODISCARD
    const path& current_path() const;

    /// Go to the next token.
    ///
    /// \code
    /// ^
    /// {        /* <- go to "a" */
    ///   "a":   /* <- go to [ */
    ///     [    /* <- go to 1 */
    ///       1, /* <- go to 2 */
    ///       2, /* ...and so on */
    ///       3,
    ///     ],
    ///   "b":
    ///     {
    ///     },
    ///   "c":
    ///     4
    /// }
    /// $
    /// \endcode
    ///
    /// \returns \c true if the reader is still \c good to read from \c current.
    JSONV_NODISCARD
    bool next_token() noexcept;

    /// Go to one past the end of the current structure.
    ///
    /// \code
    /// ^
    /// {
    ///   "a":   /* <- go to end of document */
    ///     [    /* <- go to "b" */
    ///       1, /* <- go to "b", too */
    ///       2,
    ///       3,
    ///     ],   /* <- go to "b" */
    ///   "b":   /* <- go to end of document */
    ///     {    /* <- go to "c" */
    ///     },   /* <- go to "c" */
    ///   "c":
    ///     4
    /// }        /* <- go to end of document */
    /// $
    /// \endcode
    ///
    /// The point of this over \ref next_token is abandoning a structure you are only part-way through. Once the one
    /// member you came for has been read, there is no reason to walk the rest of the object:
    ///
    /// \code
    /// /// Find the "a" member of an object and leave the rest of it unread.
    /// std::optional<std::int64_t> find_a(jsonv::reader& from)
    /// {
    ///     if (!from.expect(jsonv::ast_node_type::object_begin))
    ///         return std::nullopt;
    ///
    ///     if (!from.next_token())
    ///         return std::nullopt;
    ///
    ///     while (from.good() && from.current_type() != jsonv::ast_node_type::object_end)
    ///     {
    ///         if (!from.expect({ jsonv::ast_node_type::key_canonical, jsonv::ast_node_type::key_escaped }))
    ///             return std::nullopt;
    ///
    ///         auto key = from.current().visit_key([](const auto& k) { return std::string(k.value()); });
    ///         if (key != "a")
    ///         {
    ///             if (!from.next_key())
    ///                 return std::nullopt;
    ///
    ///             continue;
    ///         }
    ///
    ///         if (!from.next_token())
    ///             return std::nullopt;
    ///
    ///         auto node = from.current_as<jsonv::ast_node::integer>();
    ///
    ///         // Whatever is left of this object, we are done with it.
    ///         (void) from.next_structure();
    ///
    ///         if (node)
    ///             return node->value();
    ///         else
    ///             return std::nullopt;
    ///     }
    ///     return std::nullopt;
    /// }
    /// \endcode
    ///
    /// Note the call site: \c next_structure is used while sitting on a *value* inside the object, which is where it
    /// differs from \ref next_token. Called on the closing \c } itself it is merely \ref next_token, since there is no
    /// longer a structure to leave.
    ///
    /// \returns \c true if the reader is still \c good to read from \c current.
    JSONV_NODISCARD
    bool next_structure() noexcept;

    /// Go to one past the value this reader is on.
    ///
    /// Unlike \ref next_structure, which leaves the structure the reader is *inside*, this steps over the single value
    /// the reader is *on*. On a structure that means the token *after* its matching close, since the structure is the
    /// value being stepped over; on anything else it is the same as \ref next_token.
    ///
    /// \code
    /// ^
    /// {
    ///   "a":   /* <- go to [ */
    ///     [    /* <- go to "b" */
    ///       1, /* <- go to 2 */
    ///       2,
    ///       3,
    ///     ],
    ///   "b":
    ///     {    /* <- go to "c" */
    ///     },
    ///   "c":
    ///     4    /* <- go to } */
    /// }
    /// $
    /// \endcode
    ///
    /// This is the primitive for ignoring a value you do not want. Note the difference from \ref next_structure at the
    /// `4` above: this goes to the `}`, while \ref next_structure leaves the enclosing object entirely.
    ///
    /// \returns \c true if the reader is still \c good to read from \c current.
    JSONV_NODISCARD
    bool next_value() noexcept;

    /// Go to the next object key or end-of-object.
    ///
    /// \code
    /// ^
    /// {
    ///   "a":  /* <- calling here goes to "b" */
    ///     [   /* <- calling when not on an object key throws */
    ///       1,
    ///       2,
    ///       3,
    ///     ],
    ///   "b":  /* <- calling here goes to "c" */
    ///     {
    ///     },
    ///   "c":  /* <- calling here goes to end of object */
    ///     4
    /// }
    /// $
    /// \endcode
    ///
    /// \returns \c true if the reader is still \c good to read from \c current.
    /// \throws std::invalid_argument if the reader is not currently at the start of a key.
    JSONV_NODISCARD
    bool next_key();

private:
    class impl;
    class impl_parse_index;
    class impl_parse_index_owning;
    class impl_value;
    class impl_value_owning;

    template <typename TImpl, typename... TArgs>
    explicit reader(std::in_place_type_t<TImpl>, TArgs&&...);

    explicit reader(std::unique_ptr<impl> impl) noexcept;

    /// The library sometimes has to read ahead -- \c polymorphic_adapter has to find its discriminator before it knows
    /// which type to deserialize. That is a second cursor on this reader's source rather than a rewind of this one, and
    /// it is kept out of the public interface on purpose: promising it would commit every future source, including one
    /// which streams, to supporting it.
    friend class detail::reader_lookahead;

    /// Hands out a reader for each of several documents, on a tape it reuses from one to the next.
    friend class documents;

private:
    std::unique_ptr<impl> _impl;
};

/// The JSON documents in one input, read one after another, each through a \c reader of its own.
///
/// Every other way of reading JSON text takes all of it as one document, and refuses anything after its value. This
/// takes it as a sequence of documents instead: JSON Lines, with one to a line; concatenated JSON as \c jq reads and
/// writes it, as in <tt>{"a":1}{"a":2}</tt>; or a run of pretty-printed documents. Any whitespace may separate two
/// documents, as may comments where \c parse_options::comments allows them, or nothing at all where the first ends
/// with a character the second could not have continued: <tt>[1][2]</tt> and <tt>"a""b"</tt> are two documents each,
/// while <tt>12</tt> is one.
///
/// \code
/// std::ifstream file("records.jsonl");
/// for (jsonv::reader& doc : jsonv::documents(file))
///     records.push_back(jsonv::deserialize<record>(doc));
/// \endcode
///
/// Each document is handed out as a \c reader on its \c ast_node_type::document_start, as a freshly-created one is, so
/// \c jsonv::deserialize reads it as a whole document and anything else which takes a reader takes it. It is parsed
/// with the \c parse_options this was created with as though it were the only document: \c
/// parse_options::require_document and \c parse_options::max_structure_depth apply to each one. Nothing is parsed
/// until the iterator reaches it, and then onto the tape the document before it was read from, so many small documents
/// cost one tape the size of the largest rather than one each. That is also why the reader for a document is valid
/// only until the iterator moves on, wherever it has been moved to: by then its tape holds the next document.
///
/// Whitespace and comments after the last document end the sequence, so input with no value in it holds no documents.
///
/// Parsing text never throws, here as elsewhere. A document which is not valid JSON is still handed out, as a reader
/// whose \c reader::validate throws \c parse_error and from which \c jsonv::deserialize throws a
/// \c deserialization_error saying so, and the sequence ends with it: the parse stopped at the error, and nothing says
/// where that document would have ended and the next begun. The position in a \c parse_error counts from the start of
/// the whole input, not of the document it is in.
///
/// A \c std::string_view deserialized from a document views the source. A source given as a \c std::string rvalue or a
/// \c std::istream is kept by this instance, so such a view is valid for as long as it is; any other is the caller's,
/// and valid for as long as the caller keeps it.
class JSONV_PUBLIC documents final
{
public:
    class iterator;

public:
    /// \{

    /// Read the documents in JSON \a source.
    ///
    /// The \a source must stay in memory for as long as this instance and the readers it hands out are used, unless it
    /// is an rvalue reference to a \c std::string, which is moved into this instance to keep it alive. It is parsed
    /// with \a parse_options where they are given, and with the defaults \c parse_index::parse uses where they are not.
    explicit documents(std::string_view source);
    explicit documents(std::string_view source, const parse_options& parse_options);
    explicit documents(const char* source);
    explicit documents(const char* source, const parse_options& parse_options);

    explicit documents(std::string&& source);
    explicit documents(std::string&& source, const parse_options& parse_options);
    /// \}

    /// \{

    /// Read the documents in what is left of the \a source stream, as \c reader(std::istream&) reads one.
    ///
    /// The stream is read to its end here, and this instance keeps the text, so nothing reads \a source again once
    /// this returns. A \c parse_error position counts from where \a source was when this was called.
    explicit documents(std::istream& source);
    explicit documents(std::istream& source, const parse_options& parse_options);
    /// \}

    // Not copyable.
    documents(const documents&)            = delete;
    documents& operator=(const documents&) = delete;

    /// \{

    /// Moving transfers the source and the position in it. Iterators and readers already handed out are unaffected,
    /// and go on belonging to the instance moved to. The moved-from instance has no documents to give: \c begin throws
    /// \c std::invalid_argument, as using a moved-from \c reader does.
    documents(documents&&) noexcept;
    documents& operator=(documents&&) noexcept;
    /// \}

    ~documents() noexcept;

    /// Get an iterator on the first document which has not been passed yet. The first call parses the first document;
    /// any later call returns an iterator wherever the earlier ones have got to, since this is an input range and can
    /// only be walked once.
    ///
    /// \throws std::invalid_argument if this instance has been moved-from, or if the \c parse_options ask for a \c
    ///  parse_options::max_structure_depth beyond the limit, as \c parse_index::parse does.
    JSONV_NODISCARD
    iterator begin();

    /// The end of the documents, which an \c iterator compares equal to once it has passed the last one.
    JSONV_NODISCARD
    std::default_sentinel_t end() const noexcept
    {
        return std::default_sentinel;
    }

private:
    class impl;

private:
    std::unique_ptr<impl> _impl;
};

/// An input iterator over \c documents. Dereferencing it gives the \c reader for the document it is on, and
/// incrementing it parses the next one. Once it has passed the last, it compares equal to \c std::default_sentinel.
class JSONV_PUBLIC documents::iterator final
{
public:
    /// What this models in C++20 terms: an input iterator, since each document is parsed onto the tape of the last.
    using iterator_concept = std::input_iterator_tag;

    /// What dereferencing gives a reference to.
    using value_type = reader;

    /// What every iterator has to name, although stepping through documents never measures a distance.
    using difference_type = std::ptrdiff_t;

public:
    /// Create an iterator which is already at the end.
    iterator() noexcept = default;

    /// Get the reader for the document this is on. It reads the tape every document of the \c documents is parsed
    /// onto, so it is valid until this iterator, or any other from the same \c documents, is incremented.
    ///
    /// \throws std::logic_error if this iterator is at the end.
    JSONV_NODISCARD
    reader& operator*() const;

    /// \{

    /// Parse the next document and move onto it, or onto the end if there is no other.
    ///
    /// \throws std::bad_alloc if the tape cannot grow to hold the next document, which ends the sequence there.
    iterator& operator++();
    void      operator++(int) { ++*this; }
    /// \}

    /// Is \a self at the end of its \c documents?
    JSONV_NODISCARD
    friend bool operator==(const iterator& self, std::default_sentinel_t) noexcept
    {
        return self.at_end();
    }

private:
    friend class documents;

    explicit iterator(impl* owner) noexcept :
            _owner(owner)
    { }

    bool at_end() const noexcept;

private:
    impl* _owner = nullptr;
};

/// \}

}
