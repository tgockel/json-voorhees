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
#include <jsonv/encode.hpp>
#include <jsonv/writer.hpp>

#include <exception>
#include <stdexcept>
#include <string>
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

}
