/// \file
/// The exception a failed serialization throws.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization/serialization_error.hpp>
#include <jsonv/demangle.hpp>

#include <exception>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace jsonv
{

static std::string make_serialization_error_errmsg(const path&        at,
                                                   std::string_view   type_name,
                                                   const std::string& message
                                                  )
{
    std::ostringstream os;
    os << "Serialization error";

    // An empty path is the root of the document or no position at all -- a lookup which was not writing has none to
    // give -- and neither is worth naming.
    if (!at.empty())
        os << " at " << at;

    // The separator is unconditional, so the type never runs straight into the message.
    os << " serializing " << type_name << ": " << message;
    return std::move(os).str();
}

serialization_error::serialization_error(jsonv::path            path,
                                         const std::type_index& type,
                                         std::string            type_name,
                                         std::string            message,
                                         std::exception_ptr     cause
                                        ) :
        std::runtime_error(make_serialization_error_errmsg(path, type_name, message)),
        _path(std::move(path)),
        _type_index(type),
        _type_name(std::move(type_name)),
        _message(std::move(message)),
        _cause(std::move(cause))
{ }

serialization_error::serialization_error(jsonv::path            path,
                                         const std::type_index& type,
                                         std::string            message,
                                         std::exception_ptr     cause
                                        ) :
        serialization_error(std::move(path), type, demangle(type.name()), std::move(message), std::move(cause))
{ }

serialization_error::serialization_error(jsonv::path           path,
                                         const std::type_info& type,
                                         std::string           message,
                                         std::exception_ptr    cause
                                        ) :
        serialization_error(std::move(path), std::type_index(type), std::move(message), std::move(cause))
{ }

serialization_error::~serialization_error() noexcept = default;

const path& serialization_error::path() const noexcept
{
    return _path;
}

std::type_index serialization_error::type_index() const noexcept
{
    return _type_index;
}

std::string_view serialization_error::type_name() const noexcept
{
    return _type_name;
}

const std::string& serialization_error::message() const noexcept
{
    return _message;
}

const std::exception_ptr& serialization_error::nested_ptr() const noexcept
{
    return _cause;
}

}
