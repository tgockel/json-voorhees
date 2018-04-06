/** \file jsonv/detail/fallthrough.hpp
 *
 *  Copyright (c) 2018 by Travis Gockel. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
 *  as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  \author Travis Gockel (travis@gockelhut.com)
**/
#ifndef __JSONV_DETAIL_FALLTHROUGH_HPP_INCLUDED__
#define __JSONV_DETAIL_FALLTHROUGH_HPP_INCLUDED__

#include <jsonv/config.hpp>

/** \def JSONV_FALLTHROUGH
 *  Used in \c case statements that are intentionally meant to fall through to the next.
**/
#ifndef JSONV_FALLTHROUGH
#   if defined __has_cpp_attribute
#       if __has_cpp_attribute(fallthrough)
#           define JSONV_FALLTHROUGH() [[fallthrough]]
#       elif __has_cpp_attribute(clang::fallthrough)
#           define JSONV_FALLTHROUGH() [[clang::fallthrough]]
#       else
#           define JSONV_FALLTHROUGH()
#       endif
#   else
#       define JSONV_FALLTHROUGH()
#   endif
#endif

#endif/*__JSONV_DETAIL_FALLTHROUGH_HPP_INCLUDED__*/
