/// \file jsonv/optional.hpp
/// Pulls in an implementation of \c optional.
///
/// Copyright (c) 2016-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <optional>

namespace jsonv
{

/// Represents a value that may or may not be present.
template <typename T>
using optional = std::optional<T>;

namespace
{

constexpr auto& nullopt JSONV_UNUSED = std::nullopt;

}

}
