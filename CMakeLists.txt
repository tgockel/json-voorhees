# File: CMakeLists used for generating a Makefile (or whatever your build system is).
#
# Copyright 2015-2020 by Travis Gockel
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
# an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.
cmake_minimum_required(VERSION 3.20...3.29)
project(json-voorhees)

set(PACKAGE_NAME "jsonv"
    CACHE STRING "The packaging name of this library."
   )

################################################################################
# Configuration                                                                #
################################################################################

# A multi-config generator -- Visual Studio, Xcode, Ninja Multi-Config -- chooses its configuration at build time and
# leaves CMAKE_BUILD_TYPE empty on purpose. Defaulting it to Release there does not change what gets built, since the
# build tool takes the configuration from `--config`, but it does make every configure-time test below believe it is
# looking at a Release build. That is how the Windows Debug job came to be compiled with link-time optimization.
get_property(JSONV_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (NOT JSONV_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "No build type selected, default to ${CMAKE_BUILD_TYPE}")
endif()
if (JSONV_MULTI_CONFIG)
  message(STATUS "Configurations: ${CMAKE_CONFIGURATION_TYPES} (selected at build time)")
else()
  message(STATUS "Configuration: ${CMAKE_BUILD_TYPE}")
endif()

# Extract version information from the config file. It can still be overridden by the user setting JSONV_VERSION, but
# that is probably a bad idea.
file(READ include/jsonv/config.hpp JSONV_CONFIG_H_STR)
string(REGEX REPLACE ".*#define +JSONV_VERSION_MAJOR +([0-9]+).*" "\\1" JSONV_VERSION_MAJOR "${JSONV_CONFIG_H_STR}")
string(REGEX REPLACE ".*#define +JSONV_VERSION_MINOR +([0-9]+).*" "\\1" JSONV_VERSION_MINOR "${JSONV_CONFIG_H_STR}")
string(REGEX REPLACE ".*#define +JSONV_VERSION_PATCH +([0-9]+).*" "\\1" JSONV_VERSION_PATCH "${JSONV_CONFIG_H_STR}")

set(JSONV_VERSION    "${JSONV_VERSION_MAJOR}.${JSONV_VERSION_MINOR}.${JSONV_VERSION_PATCH}")
set(JSONV_SO_VERSION "${JSONV_VERSION_MAJOR}.${JSONV_VERSION_MINOR}")
message(STATUS "JSONV version: ${JSONV_VERSION} (${JSONV_SO_VERSION})")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(JSONV_BUILD_TESTS
       "Should the unit test library get built?"
       OFF
      )

##########################
# Coverage Configuration #
##########################

option(COVERAGE
       "Enable building with code coverage."
       OFF
      )
if (COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif(COVERAGE)

############################
# Sanitizer Configuration  #
############################

# Shared with the fuzz targets below so the two instrumented builds cannot drift apart.
# `-fno-sanitize-recover=all` is what turns a UBSan report into a non-zero exit instead of a printed
# warning the run would sail past.
set(JSONV_SANITIZER_FLAGS
    -g
    -fno-omit-frame-pointer
    -fsanitize=address,undefined
    -fno-sanitize-recover=all
   )

option(JSONV_SANITIZE
       "Build with AddressSanitizer and UndefinedBehaviorSanitizer."
       OFF
      )
if (JSONV_SANITIZE)
    if (MSVC)
        message(FATAL_ERROR "JSONV_SANITIZE requires a GCC- or Clang-style driver.")
    endif()

    # Directory-scoped rather than `target_compile_options(jsonv PUBLIC ...)`, which would put the
    # flags in the installed target's export set and force them on everyone who links jsonv.
    add_compile_options(${JSONV_SANITIZER_FLAGS})
    add_link_options(${JSONV_SANITIZER_FLAGS})
endif()

if (WIN32)
    # DLLs in Windows appear to have not been fully thought through
    set(DEFAULT_LIBRARY_TYPE "STATIC")
else()
    set(DEFAULT_LIBRARY_TYPE "SHARED")
endif()

set(LIBRARY_TYPE ${DEFAULT_LIBRARY_TYPE}
    CACHE STRING "The type of library to build (SHARED or STATIC)"
   )
if (LIBRARY_TYPE STREQUAL "STATIC")
    add_definitions("-DJSONV_SO=0")
endif()

option(BENCHMARK
       "Enable compilation of the json-benchmark program."
       OFF
      )

# Link-time optimization gives the compiler a chance to inline across translation units, which
# matters most for the parser hot path (parse_index.cpp + match/string.cpp + match/number.cpp).
# Default ON except for a sanitizer build, because interprocedural optimization interacts badly with
# the instrumentation. The fuzz targets turn it off for the same reason. Which configurations it
# actually applies to is decided below rather than here: a multi-config generator has no build type to
# test at configure time, so gating the option on one silently turned LTO off for the Windows Release
# build and on for the Windows Debug one.
if(JSONV_SANITIZE)
    set(JSONV_LTO_DEFAULT OFF)
else()
    set(JSONV_LTO_DEFAULT ON)
endif()
option(JSONV_LTO
       "Enable link-time / interprocedural optimization in optimized configurations."
       ${JSONV_LTO_DEFAULT}
      )
if(JSONV_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT JSONV_LTO_SUPPORTED OUTPUT JSONV_LTO_CHECK_OUTPUT)
    if(JSONV_LTO_SUPPORTED)
        # The per-configuration variables rather than the plain `CMAKE_INTERPROCEDURAL_OPTIMIZATION`, which would
        # apply to every configuration a multi-config generator builds. A Debug build compiled `/GL` and linked
        # `/LTCG` pays the link time and gives up incremental linking to optimize code nobody is measuring.
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE        TRUE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    else()
        message(WARNING "JSONV_LTO requested but the toolchain does not support it: ${JSONV_LTO_CHECK_OUTPUT}")
    endif()
endif()

option(JSONV_RELEASE_WITH_DEBUG_SYMBOLS
       "Include debug symbols (-ggdb) in Release builds."
       OFF
      )

# `JSONV_DEBUG` throttles the test suite's timing loops: without it `benchmark_tests.cpp` runs 100 iterations over the
# multi-megabyte corpus and `extract_tests.cpp` sizes its linearity guard at 20000 elements. It therefore has to follow
# the configuration being built rather than the platform. It used to be appended to `CMAKE_CXX_FLAGS_DEBUG` inside the
# non-Windows branch below, which left every MSVC build -- Debug included -- running the timing loops, against what
# both CONTRIBUTING.md and the CI workflow say happens.
#
# A generator expression rather than a `CMAKE_BUILD_TYPE` test so that multi-config generators get the right answer,
# and `add_compile_definitions` rather than a `PUBLIC` definition on the library so that it stays out of the installed
# interface. `config.hpp` documents `JSONV_DEBUG` as needing to match between the library build and the consumer's,
# which is not a choice this project should be making on a consumer's behalf.
add_compile_definitions($<$<CONFIG:Debug>:JSONV_DEBUG=1>)

if(NOT WIN32)
    # Reasonable compilers...
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -ggdb")
    if(JSONV_RELEASE_WITH_DEBUG_SYMBOLS)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ggdb")
    endif()
endif()

include_directories("${PROJECT_SOURCE_DIR}/include"
                    "${PROJECT_SOURCE_DIR}/src"
                   )

add_definitions("-DJSONV_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/src/jsonv-tests/data\"")

include(GNUInstallDirs)

configure_file(libjsonv.pc.in libjsonv.pc @ONLY)
if(WIN32)
    configure_file(packaging/nuget/jsonv.targets.in nuget/jsonv.targets @ONLY)
endif()

################################################################################
# Building                                                                     #
################################################################################

file(GLOB_RECURSE jsonv_cpps RELATIVE_PATH "." "src/jsonv/*.cpp")
add_library(jsonv ${LIBRARY_TYPE} ${jsonv_cpps})
target_compile_features(jsonv
                        PUBLIC
                          cxx_std_23
                       )
target_include_directories(jsonv
                           PUBLIC
                             "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
                             "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
                           PRIVATE
                             "${PROJECT_SOURCE_DIR}/src"
                          )
set_target_properties(jsonv
                      PROPERTIES
                        SOVERSION ${JSONV_SO_VERSION}
                        VERSION   ${JSONV_SO_VERSION}
                     )
if (JSONV_BUILD_TESTS)
    file(GLOB_RECURSE jsonv_tests_cpps RELATIVE_PATH "." "src/jsonv-tests/*.cpp")
    add_executable(jsonv-tests ${jsonv_tests_cpps})
    target_link_libraries(jsonv-tests
            "jsonv"
        )

    # Windows gives a thread 1 MB of stack where Linux and macOS give 8. `value` is a tree and its destructor
    # recurses once per level, so tearing down the 4096-deep document `reader_from_value_deep_nesting` builds
    # exhausts that in an unoptimized build -- the walk under test is a loop and is not what runs out. Nothing a
    # document can do reaches this depth: `parse` refuses anything past `parse_options::k::max_structure_depth`,
    # which is 128 and cannot be raised past its own ceiling, so only a value built in code gets this deep.
    if(MSVC)
        target_link_options(jsonv-tests PRIVATE /STACK:8388608)
    endif()

    # `allocation_counter` replaces the global `operator new` and `operator delete`, which is not something to do to
    # AddressSanitizer -- it installs its own, and taking them over costs the whole suite its `alloc-dealloc-mismatch`
    # detection. So the counter compiles itself out under a sanitizer, and the tests which assert what path tracking
    # costs go with it. That leaves the failure mode where the detection misfires somewhere it should not have, and
    # those tests quietly stop existing in a run that stays green. This is the second opinion: CMake knows whether
    # sanitizers were asked for, so it is the one thing in the build that can tell the counter it is wrong.
    if (NOT JSONV_SANITIZE)
        target_compile_definitions(jsonv-tests PRIVATE JSONV_TEST_REQUIRE_ALLOCATION_COUNTING=1)
    endif()

    add_custom_target(check
                      COMMAND $<TARGET_FILE:jsonv-tests>
                      DEPENDS jsonv-tests
                      USES_TERMINAL
                     )
endif()

if (COVERAGE)
    target_link_libraries(jsonv "gcov")
    if (JSONV_BUILD_TESTS)
        target_link_libraries(jsonv-tests "gcov")
    endif()
endif(COVERAGE)

################################################################################
# Benchmarking Tool                                                            #
################################################################################

option(BENCHMARK_JSONV
       "Enable benchmark suite for JSON Voorhees?"
       YES
      )

option(BENCHMARK_JSONCPP
       "Enable benchmark suite for JsonCpp?"
       YES
      )

option(BENCHMARK_JQ
       "Enable benchmark suite for jq?"
       NO
      )

option(BENCHMARK_JANSSON
       "Enable benchmark suite for jansson?"
       NO
      )

if (BENCHMARK)
    set(BENCHMARK_CPPS src/json-benchmark/core.cpp src/json-benchmark/main.cpp)
    set(BENCHMARK_LIBS "")
    macro(add_benchmark_suite CPP_FILE LIB_NAME)
        list(APPEND BENCHMARK_CPPS "src/json-benchmark/${CPP_FILE}")
        list(APPEND BENCHMARK_LIBS "${LIB_NAME}")
    endmacro()

    if (BENCHMARK_JSONV)
        add_benchmark_suite("jsonv_benchmark.cpp" "jsonv")
    endif()

    if (BENCHMARK_JSONCPP)
        add_benchmark_suite("jsoncpp_benchmark.cpp" "jsoncpp")
        include_directories("/usr/include/jsoncpp")
    endif()

    if (BENCHMARK_JQ)
        add_benchmark_suite("jq_benchmark.cpp" "jq")
    endif()

    if (BENCHMARK_JANSSON)
        add_benchmark_suite("jansson_benchmark.cpp" "jansson")
    endif()

    add_executable(json-benchmark ${BENCHMARK_CPPS})
    target_link_libraries(json-benchmark
        ${BENCHMARK_LIBS}
    )
endif(BENCHMARK)

################################################################################
# Fuzzing                                                                      #
################################################################################

option(JSONV_BUILD_FUZZERS
       "Build libFuzzer-based fuzz targets (Clang only)."
       OFF
      )

if (JSONV_BUILD_FUZZERS)
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR
                "JSONV_BUILD_FUZZERS requires Clang -- libFuzzer ships with the Clang toolchain. "
                "Re-run with -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++"
               )
    endif()

    # The fuzzers link a second, instrumented copy of the library rather than `jsonv` itself. Linking the normal target
    # would leave the code under test uninstrumented: libFuzzer would get no coverage feedback to guide mutation, and
    # AddressSanitizer would only see allocations, not the loads and stores inside the parser. Building separately also
    # keeps the installed target and its export set free of sanitizer flags.
    add_library(jsonv-fuzz-lib STATIC ${jsonv_cpps})
    target_include_directories(jsonv-fuzz-lib
                               PUBLIC
                                 "${PROJECT_SOURCE_DIR}/include"
                               PRIVATE
                                 "${PROJECT_SOURCE_DIR}/src"
                              )
    # The headers key `JSONV_PUBLIC` off this, so the library and the harnesses have to agree on it.
    target_compile_definitions(jsonv-fuzz-lib PUBLIC JSONV_SO=0)
    target_compile_options(jsonv-fuzz-lib
                           PUBLIC
                             ${JSONV_SANITIZER_FLAGS}
                           PRIVATE
                             # Coverage instrumentation without libFuzzer's `main`, which belongs to the executables.
                             -fsanitize=fuzzer-no-link
                          )
    target_link_options(jsonv-fuzz-lib PUBLIC ${JSONV_SANITIZER_FLAGS})

    foreach(fuzzer parse roundtrip)
        add_executable(jsonv-fuzz-${fuzzer} "src/jsonv-fuzz/${fuzzer}_fuzz.cpp")
        target_link_libraries(jsonv-fuzz-${fuzzer} PRIVATE jsonv-fuzz-lib)
        target_compile_options(jsonv-fuzz-${fuzzer} PRIVATE -fsanitize=fuzzer)
        target_link_options(jsonv-fuzz-${fuzzer} PRIVATE -fsanitize=fuzzer)
    endforeach()

    # JSONV_LTO defaults ON and turns interprocedural optimization on for the optimized configurations, which
    # interacts badly with the sanitizer and coverage instrumentation. Fuzzing does not need it. The per-configuration
    # properties have to be cleared alongside the plain one: CMake reads `INTERPROCEDURAL_OPTIMIZATION_<CONFIG>` in
    # preference to `INTERPROCEDURAL_OPTIMIZATION` when it is set, and it is set here for every target, so switching
    # off only the latter would leave these built `-flto` in the RelWithDebInfo configuration the fuzz workflow uses.
    set_target_properties(jsonv-fuzz-lib jsonv-fuzz-parse jsonv-fuzz-roundtrip
                          PROPERTIES
                            INTERPROCEDURAL_OPTIMIZATION                FALSE
                            INTERPROCEDURAL_OPTIMIZATION_RELEASE        FALSE
                            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE
                         )

    # Seeds. The JSON_Checker suite and the naughty-string list are already in the repository as test data, so they
    # are copied rather than duplicated. canada.json and citm_catalog.json are excluded on purpose: at 2.2 MB and
    # 1.7 MB they would dominate the corpus and cripple the iteration rate. Seeds with no equivalent in the test data
    # are committed under src/jsonv-fuzz/corpus and generated by src/jsonv-fuzz/make_seeds.py.
    #
    # Copying at configure time rather than from a custom target means the corpus is always there after a build,
    # instead of only when someone remembers to name an extra target. The cost is a re-configure to pick up edited
    # seeds.
    file(GLOB JSONV_FUZZ_SEEDS "src/jsonv-tests/data/json_checker/*.json"
                               "src/jsonv-tests/data/blns.json"
                               "src/jsonv-tests/data/generated.json"
                               "src/jsonv-tests/data/paths.json"
                               "src/jsonv-fuzz/corpus/*.json"
        )
    file(COPY ${JSONV_FUZZ_SEEDS} DESTINATION "${CMAKE_BINARY_DIR}/fuzz-corpus")
    list(LENGTH JSONV_FUZZ_SEEDS JSONV_FUZZ_SEED_COUNT)
    message(STATUS "Fuzz seed corpus: ${JSONV_FUZZ_SEED_COUNT} seeds in ${CMAKE_BINARY_DIR}/fuzz-corpus")
endif()

################################################################################
# Installation                                                                 #
################################################################################

message(STATUS "CMAKE_INSTALL_BINDIR == ${CMAKE_INSTALL_BINDIR}")
message(STATUS "CMAKE_INSTALL_LIBDIR == ${CMAKE_INSTALL_LIBDIR}")
message(STATUS "CMAKE_INSTALL_INCLUDEDIR == ${CMAKE_INSTALL_INCLUDEDIR}")

install(TARGETS jsonv
        EXPORT  exported
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
                COMPONENT runtime
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                COMPONENT runtime
                NAMELINK_COMPONENT development
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                COMPONENT development
       )
install(DIRECTORY   "include/jsonv"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        COMPONENT   development
       )
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libjsonv.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        COMPONENT development
       )
install(EXPORT exported
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PACKAGE_NAME}"
        FILE "${PACKAGE_NAME}-export.cmake"
        COMPONENT development
       )
if(WIN32)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/nuget/jsonv.targets"
            DESTINATION "build/native"
            COMPONENT development
           )
endif()

################################################################################
# Packaging                                                                    #
################################################################################

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32 OR APPLE)
    include(CPackComponent)

    set(CPACK_COMPONENTS_ALL runtime development)
    set(CPACK_COMPONENTS_GROUPING IGNORE)
    set(CPACK_PACKAGE_NAME "${PACKAGE_NAME}")
    set(CPACK_PACKAGE_VENDOR "Travis Gockel")
    set(CPACK_PACKAGE_CONTACT "Travis Gockel <travis@gockelhut.com>")
    set(CPACK_PACKAGE_VERSION "${JSONV_VERSION}")
    set(CPACK_PACKAGE_VERSION_MAJOR "${JSONV_VERSION_MAJOR}")
    set(CPACK_PACKAGE_VERSION_MINOR "${JSONV_VERSION_MINOR}")
    set(CPACK_PACKAGE_VERSION_PATCH "${JSONV_VERSION_PATCH}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Killer JSON for C++")
    set(CPACK_PACKAGE_DESCRIPTION "JSON Voorhees is a modern C++ library for interacting with JSON.")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://tgockel.github.io/json-voorhees/")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_VERBATIM_VARIABLES YES)

    cpack_add_component(runtime
                        DISPLAY_NAME "JSON Voorhees runtime"
                        DESCRIPTION "Shared library for JSON Voorhees."
                       )
    cpack_add_component(development
                        DISPLAY_NAME "JSON Voorhees development files"
                        DESCRIPTION "Headers and build metadata for compiling against JSON Voorhees."
                        DEPENDS runtime
                       )

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(CPACK_GENERATOR "DEB;RPM"
            CACHE STRING "CPack generators to use for binary packages."
           )
        set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

        set(CPACK_DEB_COMPONENT_INSTALL ON)
        set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
        set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
        set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
        set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
        set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
        set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY ">=")
        set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "libjsonv${JSONV_SO_VERSION}")
        set(CPACK_DEBIAN_RUNTIME_PACKAGE_SECTION libs)
        set(CPACK_DEBIAN_RUNTIME_PACKAGE_SHLIBDEPS ON)
        set(CPACK_DEBIAN_RUNTIME_DESCRIPTION "Killer modern C++ library for interacting with JSON.")
        set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_NAME "libjsonv-dev")
        set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_SECTION libdevel)
        set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_DEPENDS
            "libjsonv${JSONV_SO_VERSION} (= ${JSONV_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE})"
           )
        set(CPACK_DEBIAN_DEVELOPMENT_DESCRIPTION "Development files for JSON Voorhees.")

        set(CPACK_RPM_COMPONENT_INSTALL ON)
        set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
        set(CPACK_RPM_PACKAGE_RELEASE 1)
        set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
        set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
        set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
        set(CPACK_RPM_MAIN_COMPONENT runtime)
        set(CPACK_RPM_RUNTIME_PACKAGE_NAME "${PACKAGE_NAME}")
        set(CPACK_RPM_RUNTIME_PACKAGE_SUMMARY "Killer JSON for C++")
        set(CPACK_RPM_RUNTIME_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
        set(CPACK_RPM_DEVELOPMENT_PACKAGE_NAME "${PACKAGE_NAME}-devel")
        set(CPACK_RPM_DEVELOPMENT_PACKAGE_SUMMARY "Development files for JSON Voorhees")
        set(CPACK_RPM_DEVELOPMENT_PACKAGE_DESCRIPTION "Headers and build metadata for compiling against JSON Voorhees.")
        set(CPACK_RPM_DEVELOPMENT_PACKAGE_REQUIRES "${PACKAGE_NAME} = ${JSONV_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}")
    elseif(WIN32)
        set(CPACK_GENERATOR "NuGet"
            CACHE STRING "CPack generators to use for binary packages."
           )
        set(CPACK_PACKAGING_INSTALL_PREFIX "")
        set(CPACK_NUGET_PACKAGE_NAME "${PACKAGE_NAME}")
        set(CPACK_NUGET_PACKAGE_VERSION "${JSONV_VERSION}")
        set(CPACK_NUGET_PACKAGE_TITLE "JSON Voorhees")
        set(CPACK_NUGET_PACKAGE_AUTHORS "Travis Gockel")
        set(CPACK_NUGET_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
        set(CPACK_NUGET_PACKAGE_HOMEPAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
        set(CPACK_NUGET_PACKAGE_LICENSE_EXPRESSION "Apache-2.0")
        set(CPACK_NUGET_PACKAGE_TAGS "native C++ JSON jsonv")
        set(CPACK_NUGET_PACKAGE_REPOSITORY_URL
            "https://github.com/tgockel/json-voorhees.git"
           )
        set(CPACK_NUGET_PACKAGE_REPOSITORY_TYPE git)
    elseif(APPLE)
        set(CPACK_GENERATOR "productbuild"
            CACHE STRING "CPack generators to use for binary packages."
           )
        set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
                       "${CMAKE_CURRENT_BINARY_DIR}/CPackLicense.txt"
                       COPYONLY
                      )
        set(CPACK_RESOURCE_FILE_LICENSE
            "${CMAKE_CURRENT_BINARY_DIR}/CPackLicense.txt"
           )
        set(CPACK_PRODUCTBUILD_IDENTIFIER "com.tgockel.jsonv")
        set(CPACK_PRODUCTBUILD_DOMAINS ON)
        set(CPACK_PRODUCTBUILD_DOMAINS_ANYWHERE OFF)
        set(CPACK_PRODUCTBUILD_DOMAINS_USER OFF)
        set(CPACK_PRODUCTBUILD_DOMAINS_ROOT ON)
    endif()

    include(CPack)
endif()
