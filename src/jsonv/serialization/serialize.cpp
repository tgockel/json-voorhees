/// \file
/// Serialization of C++ types into a JSON token stream.
///
/// Copyright (c) 2015-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization/serialize.hpp>
#include <jsonv/demangle.hpp>
#include <jsonv/detail/scope_exit.hpp>
#include <jsonv/encode.hpp>
#include <jsonv/writer.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// serialization_context                                                                                              //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

serialization_context::serialization_context(jsonv::formats                fmt,
                                             std::optional<jsonv::version> ver,
                                             const void*                   userdata
                                            ) :
        context(std::move(fmt), ver, userdata) // NOLINT(performance-move-const-arg): formats is not movable yet (#286)
{ }

serialization_context::serialization_context() :
        context()
{ }

serialization_context::~serialization_context() noexcept = default;

void serialization_context::serialize(const std::type_info& type, const void* from, writer& to) const
{
    // A moved-from writer refuses every token, which is the failure being translated here; asking it where it is would
    // only throw again out of the handler.
    auto where = [&]() { return to.good() ? to.current_path() : jsonv::path(); };

    try
    {
        formats().serialize(type, from, to, *this);
    }
    catch (const serialization_error&)
    {
        // Already says where it is and what was being serialized there: a no_serializer from the lookup, or an error
        // out of a serializer further down. Wrapping it again would put this frame's type and position, which are
        // less precise, in front of those.
        throw;
    }
    catch (const std::exception& ex)
    {
        throw serialization_error(where(), type, ex.what(), std::current_exception());
    }
    catch (...)
    {
        throw serialization_error(where(),
                                  type,
                                  std::string("Exception with type ") + current_exception_type_name(),
                                  std::current_exception()
                                 );
    }
}

value serialization_context::to_json(const std::type_info& type, const void* from) const
{
    // The sink before the writer, which keeps a pointer to it, so that the writer is destroyed first.
    value_encoder sink;
    writer        to(sink);
    serialize(type, from, to);

    try
    {
        return std::move(sink).take();
    }
    catch (const std::logic_error& ex)
    {
        // The serializer returned without writing a value, or with a structure still open. That is its bug rather than
        // the caller's, and naming the type is what makes it findable.
        throw serialization_error(to.current_path(), type, ex.what(), std::current_exception());
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// serialize                                                                                                          //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace
{

/// The compact encoder <tt>writer(std::ostream&)</tt> owns, which also counts the values begun at the root. The writer
/// can say that every structure is closed, but neither that anything was written nor that a second value followed the
/// first: a writer at depth zero takes another root, which is how two documents end up in one stream. Every value
/// reaches one of the hooks overridden here, a whole \c value handed to \c writer::write included, since the default
/// \c encoder::write_tree walks it through them.
class document_encoder final :
        public ostream_encoder
{
public:
    explicit document_encoder(std::ostream& output) :
            ostream_encoder(output)
    { }

    /// The values begun at the root so far, the last of which may still be open.
    JSONV_NODISCARD
    std::size_t roots() const noexcept { return _roots; }

protected:
    void write_null() override
    {
        write_token(token_kind::scalar, [this] { ostream_encoder::write_null(); });
    }

    void write_object_begin() override
    {
        write_token(token_kind::open, [this] { ostream_encoder::write_object_begin(); });
    }

    void write_object_end() override
    {
        write_token(token_kind::close, [this] { ostream_encoder::write_object_end(); });
    }

    void write_array_begin() override
    {
        write_token(token_kind::open, [this] { ostream_encoder::write_array_begin(); });
    }

    void write_array_end() override
    {
        write_token(token_kind::close, [this] { ostream_encoder::write_array_end(); });
    }

    void write_string(std::string_view value) override
    {
        write_token(token_kind::scalar, [&] { ostream_encoder::write_string(value); });
    }

    void write_integer(std::int64_t value) override
    {
        write_token(token_kind::scalar, [&] { ostream_encoder::write_integer(value); });
    }

    void write_decimal(double value) override
    {
        write_token(token_kind::scalar, [&] { ostream_encoder::write_decimal(value); });
    }

    void write_boolean(bool value) override
    {
        write_token(token_kind::scalar, [&] { ostream_encoder::write_boolean(value); });
    }

private:
    enum class token_kind : unsigned char
    {
        scalar,
        open,
        close,
    };

    /// Write one token through \a write and count it. \c ostream_encoder writes some tokens through its other hooks --
    /// a non-finite decimal as \c write_null, a key through \c write_string -- and a hook called from inside another is
    /// part of the token already being written, not one of its own.
    template <typename FWrite>
    void write_token(token_kind kind, const FWrite& write)
    {
        if (_writing)
        {
            write();
            return;
        }

        _writing  = true;
        auto done = detail::on_scope_exit([this] { _writing = false; });
        write();

        if (kind == token_kind::close)
        {
            --_depth;
        }
        else
        {
            if (_depth == 0U)
                ++_roots;
            if (kind == token_kind::open)
                ++_depth;
        }
    }

private:
    std::size_t _depth   = 0U;
    std::size_t _roots   = 0U;
    bool        _writing = false;
};

}

void detail::serialize_document(const serialization_context& context,
                                const std::type_info&        type,
                                const void*                  from,
                                std::ostream&                to
                               )
{
    // The sink before the writer, which keeps a pointer to it, so that the writer is destroyed first.
    document_encoder sink(to);
    writer           out(sink);
    context.serialize(type, from, out);

    // The two checks `value_encoder::take` makes for `to_json`, in its order, so that a serializer which cannot produce
    // a document is refused the same way whichever entry point it is run through. Then one `take` does not make: a
    // `value_encoder` keeps the last of two roots, where text has already run the two together.
    std::string problem;
    if (out.depth() > 0U)
        problem = "The document is incomplete: " + std::to_string(out.depth()) + " structure(s) still open";
    else if (sink.roots() == 0U)
        problem = "The document is empty: nothing has been written";
    else if (sink.roots() > 1U)
        problem = "The document is more than one value: " + std::to_string(sink.roots()) + " were written";

    if (!problem.empty())
    {
        auto cause = std::make_exception_ptr(std::logic_error(problem));
        throw serialization_error(out.current_path(), type, std::move(problem), std::move(cause));
    }
}

std::string detail::serialize_to_string(const serialization_context& context,
                                        const std::type_info&        type,
                                        const void*                  from
                                       )
{
    std::ostringstream text;
    serialize_document(context, type, from, text);
    return std::move(text).str();
}

}
