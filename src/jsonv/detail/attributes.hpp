/// \file jsonv/detail/attributes.hpp
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#pragma once

/// \def JSONV_ALWAYS_INLINE
/// \brief Always inline the function this decorates, no matter what the compiler might think is best.
#ifndef JSONV_ALWAYS_INLINE
#   if defined(__GNUC__)
#       define JSONV_ALWAYS_INLINE __attribute__((always_inline))
#   else
#       define JSONV_ALWAYS_INLINE
#   endif
#endif

/// \def JSONV_NOINLINE
/// \brief Never inline the function this decorates. For a rarely taken path, so it does not bloat its hot callers.
#ifndef JSONV_NOINLINE
#   if defined(__GNUC__)
#       define JSONV_NOINLINE __attribute__((noinline))
#   elif defined(_MSC_VER)
#       define JSONV_NOINLINE __declspec(noinline)
#   else
#       define JSONV_NOINLINE
#   endif
#endif

/// \def JSONV_LIKELY
/// Mark that a section of code is likely to be reached.
///
/// \see JSONV_UNLIKELY
#ifndef JSONV_LIKELY
#   define JSONV_LIKELY [[likely]]
#endif

/// \def JSONV_UNLIKELY
/// Mark that a section of code is not likely to be reached.
///
/// \see JSONV_LIKELY
#ifndef JSONV_UNLIKELY
#   define JSONV_UNLIKELY [[unlikely]]
#endif
