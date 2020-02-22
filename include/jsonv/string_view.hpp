/// \file jsonv/string_view.hpp
/// Pulls in an implementation of \c string_view.
///
/// Copyright (c) 2014-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

#include <string_view>

namespace jsonv
{

/// A non-owning reference to a string.
using string_view = std::string_view;

}
