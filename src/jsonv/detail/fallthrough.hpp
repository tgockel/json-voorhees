/// \file jsonv/detail/fallthrough.hpp
///
/// Copyright (c) 2018-2020 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

#include <jsonv/config.hpp>

namespace jsonv
{

/// \def JSONV_FALLTHROUGH
/// Used in \c case statements that are intentionally meant to fall through to the next.
#ifndef JSONV_FALLTHROUGH
#   define JSONV_FALLTHROUGH() [[fallthrough]]
#endif

}
