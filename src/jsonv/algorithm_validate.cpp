/** \file
 *
 *  Copyright (c) 2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#include <jsonv/algorithm.hpp>

#include <sstream>

namespace jsonv
{

static std::string validation_error_whatstring(validation_error::code code, const path& p, const value& elem)
{
    std::ostringstream ss;
    ss << "Validation error: Got " << code << " at path " << p << ": " << elem;
    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const validation_error::code& code)
{
    switch (code)
    {
    case validation_error::code::non_finite_number: return os << "non-finite number";
    default:                                        return os << "validation_error::code(" << static_cast<int>(code) << ")";
    }
}

validation_error::validation_error(code code_, jsonv::path path_, jsonv::value value_) :
        runtime_error(validation_error_whatstring(code_, path_, value_)),
        _code(code_),
        _path(std::move(path_)),
        _value(std::move(value_))
{ }

validation_error::~validation_error() noexcept = default;

validation_error::code validation_error::error_code() const
{
    return _code;
}

const path& validation_error::path() const
{
    return _path;
}

const value& validation_error::value() const
{
    return _value;
}

void validate(const value& val)
{
    traverse(val,
             [] (const path& p, const value& elem)
             {
                 if (elem.kind() == kind::decimal)
                 {
                     if (!std::isfinite(elem.as_decimal()))
                         throw validation_error(validation_error::code::non_finite_number, p, elem);
                 }
             }
            );
}

}
