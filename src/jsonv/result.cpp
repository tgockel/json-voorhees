/// \file jsonv/result.hpp
///
/// Copyright (c) 2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/result.hpp>

#include <ostream>
#include <sstream>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// result_state                                                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const result_state& state)
{
    switch (state)
    {
    case result_state::empty: return os << "empty";
    case result_state::ok:    return os << "ok";
    case result_state::error: return os << "error";
    default:                  return os << "result_state(" << +static_cast<unsigned char>(state) << ")";
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// bad_result_access                                                                                                  //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bad_result_access::bad_result_access(const std::string& description) noexcept :
        std::logic_error(description)
{ }

bad_result_access bad_result_access::must_be(string_view op_name, result_state expected, result_state actual)
{
    std::ostringstream os;
    os << "`" << op_name << "` requires state of " << expected << ", but the instance has state of " << actual;
    return bad_result_access(std::move(os).str());
}

bad_result_access bad_result_access::must_not_be(string_view op_name, result_state unexpected)
{
    std::ostringstream os;
    os << "`" << op_name << "` can not be performed on result with state " << unexpected;
    return bad_result_access(std::move(os).str());
}

bad_result_access::~bad_result_access() noexcept = default;

}
