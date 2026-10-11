# File: CMakeLists.txt -- the top of the build. The library, tests, benchmark and fuzzers each have a CMakeLists.txt
# of their own under src/; reading the version, installing and packaging live in cmake/.
#
# Copyright 2015-2026 by Travis Gockel
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
# an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.
cmake_minimum_required(VERSION 3.23...4.2)

# include/jsonv/config.hpp is the one place the version is written down. It is read before project() so that the
# VERSION argument -- and with it PROJECT_VERSION, the package version file and CPack -- comes from there.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/JsonvVersion.cmake")
jsonv_read_version("${CMAKE_CURRENT_SOURCE_DIR}/include/jsonv/config.hpp" JSONV_VERSION)

project(json-voorhees
        VERSION      ${JSONV_VERSION}
        DESCRIPTION  "Killer JSON for C++"
        HOMEPAGE_URL "https://tgockel.github.io/json-voorhees/"
        LANGUAGES    CXX
       )
message(STATUS "JSONV version: ${PROJECT_VERSION}")

################################################################################
# Options                                                                      #
################################################################################

# Defaults follow PROJECT_IS_TOP_LEVEL: a stand-alone build gets the tests and install rules, while a project which
# pulled jsonv in through add_subdirectory or FetchContent gets the library and nothing else unless it asks.
option(JSONV_BUILD_TESTS
       "Build jsonv-tests and register it with CTest."
       ${PROJECT_IS_TOP_LEVEL}
      )
option(JSONV_BUILD_BENCHMARK
       "Build the json-benchmark program."
       OFF
      )
option(JSONV_BUILD_FUZZERS
       "Build the libFuzzer targets (Clang only)."
       OFF
      )
option(JSONV_INSTALL
       "Generate install rules, and CPack configuration when top-level."
       ${PROJECT_IS_TOP_LEVEL}
      )
option(JSONV_SANITIZE
       "Build this project's targets with AddressSanitizer and UndefinedBehaviorSanitizer."
       OFF
      )
option(JSONV_COVERAGE
       "Build this project's targets with gcov instrumentation."
       OFF
      )
option(JSONV_RELEASE_WITH_DEBUG_SYMBOLS
       "Include debug symbols (-ggdb) in Release builds of this project's targets."
       OFF
      )

# Link-time optimization defaults on for a stand-alone build. Off under a sanitizer, because interprocedural
# optimization interacts badly with the instrumentation, and off as a subproject: a static libjsonv.a of Clang LTO
# bitcode cannot be linked by a consumer whose own link line lacks -flto, and whether to pay for LTO is the
# consumer's call anyway.
if(PROJECT_IS_TOP_LEVEL AND NOT JSONV_SANITIZE)
    set(JSONV_LTO_DEFAULT ON)
else()
    set(JSONV_LTO_DEFAULT OFF)
endif()
option(JSONV_LTO
       "Enable link-time optimization for jsonv in the Release and RelWithDebInfo configurations."
       ${JSONV_LTO_DEFAULT}
      )

# The library type follows the standard variable. Stand-alone and unset, it is shared everywhere but Windows, which
# is what the deb, rpm, pkg and NuGet packages ship (DLLs in Windows appear to have not been fully thought through).
# As a subproject the consumer's value, or CMake's static default, wins.
if(PROJECT_IS_TOP_LEVEL AND NOT DEFINED BUILD_SHARED_LIBS)
    if(WIN32)
        set(JSONV_SHARED_DEFAULT OFF)
    else()
        set(JSONV_SHARED_DEFAULT ON)
    endif()
    option(BUILD_SHARED_LIBS "Build jsonv as a shared library." ${JSONV_SHARED_DEFAULT})
endif()

# Options from before every option carried the JSONV_ prefix. A CMakeCache.txt written by an older checkout still
# has them, and a cache which appears to ask for something the build no longer reads is worse than a warning.
if(PROJECT_IS_TOP_LEVEL)
    foreach(rename IN ITEMS "LIBRARY_TYPE=BUILD_SHARED_LIBS"
                            "BENCHMARK=JSONV_BUILD_BENCHMARK"
                            "BENCHMARK_JSONV=JSONV_BENCHMARK_JSONV"
                            "BENCHMARK_JSONCPP=JSONV_BENCHMARK_JSONCPP"
                            "BENCHMARK_JQ=JSONV_BENCHMARK_JQ"
                            "BENCHMARK_JANSSON=JSONV_BENCHMARK_JANSSON"
                            "COVERAGE=JSONV_COVERAGE"
                            "PACKAGE_NAME="
           )
        string(REGEX MATCH "^([^=]+)=(.*)$" _ "${rename}")
        set(old "${CMAKE_MATCH_1}")
        set(new "${CMAKE_MATCH_2}")
        if(DEFINED CACHE{${old}})
            if(new)
                message(WARNING "${old} is no longer a jsonv build option; set ${new} instead. Removing it from the cache.")
            else()
                message(WARNING "${old} is no longer a jsonv build option; the package is always named jsonv. Removing it from the cache.")
            endif()
            unset(${old} CACHE)
        endif()
    endforeach()
endif()

################################################################################
# Settings only a top-level build may make                                     #
################################################################################

if(PROJECT_IS_TOP_LEVEL)
    # A multi-config generator -- Visual Studio, Xcode, Ninja Multi-Config -- chooses its configuration at build
    # time and leaves CMAKE_BUILD_TYPE empty on purpose. Defaulting it there would not change what gets built, but
    # it would make every configure-time test believe it is looking at a Release build.
    get_property(JSONV_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(JSONV_MULTI_CONFIG)
        message(STATUS "Configurations: ${CMAKE_CONFIGURATION_TYPES} (selected at build time)")
    else()
        if(NOT CMAKE_BUILD_TYPE)
            set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type." FORCE)
            message(STATUS "No build type selected, defaulting to ${CMAKE_BUILD_TYPE}")
        endif()
        if(DEFINED CACHE{CMAKE_BUILD_TYPE})
            set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
        endif()
        message(STATUS "Configuration: ${CMAKE_BUILD_TYPE}")
    endif()

    # clang-tidy and editors read the compile flags from compile_commands.json. Generators other than Makefiles and
    # Ninja ignore this.
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # The targets live in subdirectories, but their artifacts stay where CI, scripts/perf-bench.sh and the docs look
    # for them: build/libjsonv.so, build/jsonv-tests, build/jsonv-fuzz-parse, and build/<Config>/jsonv-tests.exe
    # under a multi-config generator, which appends the configuration itself. A subproject's layout is the
    # consumer's to decide.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
endif()

# These initialise per-target properties for the targets defined under this directory and do not escape to a parent
# project. The requirement a consumer sees is the PUBLIC cxx_std_23 compile feature on the library target.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/JsonvDevFlags.cmake")

################################################################################
# Targets                                                                      #
################################################################################

add_subdirectory(src/jsonv)

if(JSONV_BUILD_TESTS)
    # Here rather than in src/jsonv-tests: ctest looks for CTestTestfile.cmake at the build root, which only the
    # directory that calls enable_testing() -- and everything below it -- gets.
    enable_testing()
    add_subdirectory(src/jsonv-tests)
endif()

if(JSONV_BUILD_BENCHMARK)
    add_subdirectory(src/json-benchmark)
endif()

if(JSONV_BUILD_FUZZERS)
    add_subdirectory(src/jsonv-fuzz)
endif()

################################################################################
# Installation and Packaging                                                   #
################################################################################

if(JSONV_INSTALL)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/JsonvInstall.cmake")
    if(PROJECT_IS_TOP_LEVEL)
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/JsonvPackaging.cmake")
    endif()
endif()
