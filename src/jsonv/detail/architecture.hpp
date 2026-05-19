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

// AVX2 is gated at runtime via __builtin_cpu_supports + __attribute__((target("avx2"))).
// It does not require -mavx2 at the TU level; it only requires that the compiler
// supports the target attribute and that <immintrin.h> is reachable -- both true on
// GCC/Clang for x86. We piggy-back on JSONV_SSE2 since every AVX2-capable CPU also
// has SSE2.
#ifndef JSONV_AVX2
#   if JSONV_SSE2 && (defined(__GNUC__) || defined(__clang__))
#       define JSONV_AVX2 1
#   else
#       define JSONV_AVX2 0
#   endif
#endif
