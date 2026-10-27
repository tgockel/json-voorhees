/// \file
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/parse.hpp>
#include <jsonv/parse_index.hpp>
#include <jsonv/reader.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "detail.hpp"
#include "reader_impl_parse_index.hpp"

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// documents::impl                                                                                                    //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Everything a \c documents reads with, kept on the heap so that moving the \c documents moves none of it: \c _source
/// may view \c _owned_source, which a move would leave behind if it is short, and the tape and the reader both hold
/// positions in \c _source.
class JSONV_LOCAL documents::impl final
{
public:
    explicit impl(std::string_view source, const parse_options& options) :
            _source(source),
            _options(options)
    { }

    explicit impl(std::string&& source, const parse_options& options) :
            _owned_source(std::move(source)),
            _source(_owned_source),
            _options(options)
    { }

    impl(const impl&)            = delete;
    impl& operator=(const impl&) = delete;

    /// Parse the first document, if that has not been done already.
    void start()
    {
        if (!_started)
        {
            _started = true;
            advance();
        }
    }

    /// Parse the next document onto the tape and hand out a reader on it, or end the sequence if there is no other.
    void advance();

    bool at_end() const noexcept
    {
        return !_current;
    }

    reader& current()
    {
        if (_current)
            return *_current;
        else
            throw std::logic_error("documents::iterator dereferenced at the end");
    }

private:
    /// Empty unless the source was handed over to be kept.
    std::string                _owned_source;
    std::string_view           _source;
    parse_options              _options;
    /// Every document is parsed onto this in turn.
    parse_index                _tape;
    /// Where the next document may begin; or nothing once the sequence has ended.
    std::optional<std::size_t> _next    = 0U;
    bool                       _started = false;
    /// The reader on the document the tape holds; or nothing at the end. Declared after \c _tape, which it reads.
    std::optional<reader>      _current;
};

void documents::impl::advance()
{
    // Whatever happens, the reader on the document before is done with: the tape is about to hold something else.
    _current.reset();

    // Taken rather than read, so that a parse which throws ends the sequence instead of leaving it to be tried again.
    auto from = std::exchange(_next, std::nullopt);
    if (!from)
        return;

    auto stop = _tape.parse_next(_source, *from, _options);
    if (!stop)
        return;

    // Not `make_unique`, which cannot reach the private constructor.
    _current = reader(std::unique_ptr<reader::impl>(new reader::impl_parse_index(_tape, _tape.begin())));

    // A document which failed to parse is the last. The parse stopped at the error, so nothing says where the document
    // would have ended, and reading on from the error would report whatever the rest of it looks like out of context.
    if (_tape.success())
        _next = stop;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// documents                                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

documents::documents(std::string_view source) :
        documents(source, parse_options::create_default())
{ }

documents::documents(std::string_view source, const parse_options& options) :
        _impl(std::make_unique<impl>(source, options))
{ }

documents::documents(const char* source) :
        documents(std::string_view(source))
{ }

documents::documents(const char* source, const parse_options& options) :
        documents(std::string_view(source), options)
{ }

documents::documents(std::string&& source) :
        documents(std::move(source), parse_options::create_default())
{ }

documents::documents(std::string&& source, const parse_options& options) :
        _impl(std::make_unique<impl>(std::move(source), options))
{ }

documents::documents(std::istream& source) :
        documents(read_stream(source))
{ }

documents::documents(std::istream& source, const parse_options& options) :
        documents(read_stream(source), options)
{ }

documents::documents(documents&&) noexcept            = default;
documents& documents::operator=(documents&&) noexcept = default;

documents::~documents() noexcept = default;

documents::iterator documents::begin()
{
    if (!_impl)
        throw std::invalid_argument("documents instance has been moved-from");

    _impl->start();
    return iterator(_impl.get());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// documents::iterator                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

reader& documents::iterator::operator*() const
{
    if (_owner)
        return _owner->current();
    else
        throw std::logic_error("documents::iterator dereferenced at the end");
}

documents::iterator& documents::iterator::operator++()
{
    if (_owner)
        _owner->advance();

    return *this;
}

bool documents::iterator::at_end() const noexcept
{
    return !_owner || _owner->at_end();
}

}
