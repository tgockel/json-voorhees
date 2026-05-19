/// \file
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)

#include <jsonv/config.hpp>

#ifndef JSONV_SSE2
#   if defined __SSE2__ && __SSE2__
#       define JSONV_SSE2 1
#   else
#       define JSONV_SSE2 0
#   endif
#endif
