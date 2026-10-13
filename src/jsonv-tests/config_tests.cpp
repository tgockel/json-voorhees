/// \file
/// Tests for the version macros in \c jsonv/config.hpp.
///
/// Copyright (c) 2026 by Travis Gockel. All rights reserved.
///
/// This program is free software: you can redistribute it and/or modify it under the terms of the Apache License
/// as published by the Apache Software Foundation, either version 2 of the License, or (at your option) any later
/// version.
///
/// \author Travis Gockel (travis@gockelhut.com)
#include "test.hpp"

#include <jsonv/config.hpp>

#include <string>
#include <string_view>

namespace jsonv_test
{

// `JSONV_TEST_VERSION` is what `cmake/JsonvVersion.cmake` read out of config.hpp with a regex, and it named the
// packages, `libjsonv.pc` and the shared library. The compiler reads the same header its own way; the two must agree.
TEST(config_version_matches_build)
{
    constexpr std::string_view prerelease = JSONV_VERSION_PRERELEASE;

    std::string from_macros = std::to_string(JSONV_VERSION_MAJOR) + '.' + std::to_string(JSONV_VERSION_MINOR) + '.'
                            + std::to_string(JSONV_VERSION_PATCH);
    if (!prerelease.empty())
        from_macros += "-" + std::string(prerelease);

    ensure_eq(std::string(JSONV_TEST_VERSION), from_macros);
}

}
