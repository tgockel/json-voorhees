/// \file
/// Conversion between C++ types and JSON values.
///
/// Copyright (c) 2015-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include <jsonv/serialization.hpp>

namespace jsonv
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// serializer                                                                                                         //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

serializer::~serializer() noexcept = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// adapter                                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

adapter::~adapter() noexcept = default;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// context                                                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

context::context(jsonv::formats                fmt,
                 std::optional<jsonv::version> ver,
                 const void*                   userdata
                ) :
        _formats(std::move(fmt)), // NOLINT(performance-move-const-arg): `formats` cannot be moved yet (#286)
        _version(ver),
        _user_data(userdata)
{ }

context::context() :
        context(formats::global())
{ }

context::~context() noexcept = default;

}
