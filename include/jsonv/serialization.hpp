/// \file jsonv/serialization.hpp
/// Conversion between C++ types and JSON values.
///
/// Copyright (c) 2015-2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>
#include <jsonv/serialization/adapter.hpp>
#include <jsonv/serialization/context.hpp>
#include <jsonv/serialization/deserialize.hpp>
#include <jsonv/serialization/formats.hpp>
#include <jsonv/serialization/serialize.hpp>
#include <jsonv/serialization/serializer.hpp>
#include <jsonv/value.hpp>
#include <jsonv/version.hpp>

namespace jsonv
{

/// \addtogroup Serialization
/// \{
/// Serialization components are responsible for conversion between a C++ type and JSON, as a \c value or as a stream
/// of tokens.
/// \}

}
