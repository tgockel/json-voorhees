# File: benchmarks/nativejson/CMakeLists.txt -- nativejson-benchmark (https://github.com/miloyip/nativejson-benchmark)
# against this checkout of JSON Voorhees. A project of its own rather than an option on the main build: it fetches
# the harness and the competing libraries, and nothing in it should touch a normal build of jsonv. See README.md.
#
# Copyright 2026 by Travis Gockel
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
# an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.
cmake_minimum_required(VERSION 3.25...4.2)

project(jsonv-nativejson LANGUAGES CXX)

option(NATIVEJSON_MARCH_NATIVE
       "Compile everything, jsonv included, with -march=native as upstream's premake build does."
       ON
      )

get_property(NATIVEJSON_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT NATIVEJSON_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type." FORCE)
    message(STATUS "No build type selected, defaulting to ${CMAKE_BUILD_TYPE}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Before jsonv is added, so the library is compiled the same way as the header-only libraries it is measured against.
if(NATIVEJSON_MARCH_NATIVE AND NOT MSVC)
    add_compile_options(-march=native)
endif()

################################################################################
# JSON Voorhees                                                                #
################################################################################

# Static, so the code size rows compare like with like: every other library here is compiled into the program.
set(BUILD_SHARED_LIBS OFF)
set(JSONV_BUILD_TESTS OFF)
set(JSONV_INSTALL     OFF)
set(JSONV_LTO         ON)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." jsonv EXCLUDE_FROM_ALL)

# jsonv's archive is LTO bitcode under Clang, which only links into a program that is itself built with -flto.
include(CheckIPOSupported)
check_ipo_supported(RESULT NATIVEJSON_IPO_SUPPORTED LANGUAGES CXX)
if(NATIVEJSON_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE        ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
endif()

################################################################################
# Third-party sources                                                          #
################################################################################

# Each is populated only: SOURCE_SUBDIR names a directory with no CMakeLists.txt, so none of their builds run here.
include(FetchContent)
FetchContent_Declare(nativejson
                     URL           https://github.com/miloyip/nativejson-benchmark/archive/478d5727c2a4048e835a29c65adecc7d795360d5.tar.gz
                     URL_HASH      SHA256=94411f5e549028c45253eb6df8b060ef16c121d75f11c3f497311357e8b6a668
                     SOURCE_SUBDIR populate-only
                    )
# The last release, 1.1.0, is from 2016 and no longer compiles with current GCC.
FetchContent_Declare(rapidjson
                     URL           https://github.com/Tencent/rapidjson/archive/24b5e7a8b27f42fa16b96fc70aade9106cf7102f.tar.gz
                     URL_HASH      SHA256=2d2601a82d2d3b7e143a3c8d43ef616671391034bc46891a9816b79cf2d3e7a8
                     SOURCE_SUBDIR populate-only
                    )
FetchContent_Declare(nlohmann_json
                     URL           https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
                     URL_HASH      SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
                     SOURCE_SUBDIR populate-only
                    )
FetchContent_Declare(simdjson
                     URL           https://github.com/simdjson/simdjson/archive/refs/tags/v5.0.5.tar.gz
                     URL_HASH      SHA256=50bb17769e1054ff1bfcd5dbbfcf33544c77f6ebdb31a318436ea0e47f78f571
                     SOURCE_SUBDIR populate-only
                    )
FetchContent_MakeAvailable(nativejson rapidjson nlohmann_json simdjson)

# Boost.JSON is header-only through <boost/json/src.hpp>, but Boost is too large to fetch for one benchmark row.
find_package(Boost 1.75 CONFIG COMPONENTS headers)
if(NOT Boost_FOUND)
    message(STATUS "Boost not found; leaving Boost.JSON out of the benchmark")
endif()

################################################################################
# Harness                                                                      #
################################################################################

set(NATIVEJSON_SRC "${nativejson_SOURCE_DIR}/src")
set(NATIVEJSON_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")

# BenchCodeSize takes everything from the first '_' in the program's name as the suffix of the jsonstat programs, the
# way upstream's premake names them, and passes strchr's NULL to printf when there is none. This program's name has
# no '_' and the jsonstat programs have no suffix.
file(READ "${NATIVEJSON_SRC}/main.cpp" nativejson_main)
set(nativejson_suffix_old "const char* filename_suffix = strchr(lastSep + 1, '_');")
set(nativejson_suffix_new "const char* filename_suffix = \"\";")
string(FIND "${nativejson_main}" "${nativejson_suffix_old}" nativejson_suffix_at)
if(nativejson_suffix_at EQUAL -1)
    message(FATAL_ERROR "Upstream main.cpp no longer computes filename_suffix the way this build patches it")
endif()
string(REPLACE "${nativejson_suffix_old}" "${nativejson_suffix_new}" nativejson_main "${nativejson_main}")
file(CONFIGURE OUTPUT "${NATIVEJSON_GENERATED}/main.cpp" CONTENT "${nativejson_main}" @ONLY)

# The same cleanup of the CPU name as upstream's build/machine.sh: it becomes part of the result file's name.
set(NATIVEJSON_MACHINE "unknown")
if(EXISTS "/proc/cpuinfo")
    file(STRINGS "/proc/cpuinfo" nativejson_model_name REGEX "^model name" LIMIT_COUNT 1)
    if(nativejson_model_name)
        string(REGEX REPLACE "^model name[^:]*:" "" nativejson_model_name "${nativejson_model_name}")
        string(REGEX REPLACE "[ \t]+|Intel\\(R\\)|\\(TM\\)|\\(R\\)|CPU" "" nativejson_model_name "${nativejson_model_name}")
        set(NATIVEJSON_MACHINE "${nativejson_model_name}")
    endif()
endif()
configure_file(machine.h.in "${NATIVEJSON_GENERATED}/machine.h" @ONLY)

# Upstream's adapter includes the header from where its 2016 submodule kept it.
file(CONFIGURE OUTPUT "${NATIVEJSON_GENERATED}/nlohmann/src/json.hpp"
     CONTENT "#pragma once\n#include <nlohmann/json.hpp>\n"
    )

# main.cpp reads ../data relative to its working directory, which is run/.
file(COPY "${nativejson_SOURCE_DIR}/data" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/run")

################################################################################
# Programs                                                                     #
################################################################################

# The name of each adapter's source file, less its extension, is the name BenchCodeSize looks for its jsonstat program
# under. RapidJSON is not optional: main.cpp reads no data unless it is there to compute the reference statistics.
# Upstream's adapters are used as they are where they still compile against the current library; the copies here say
# at the top what they change.
set(nativejson_adapters
    "${CMAKE_CURRENT_SOURCE_DIR}/jsonvtest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/simdjsontest.cpp"
    "${NATIVEJSON_SRC}/tests/nlohmanntest.cpp"
    "${NATIVEJSON_SRC}/tests/rapidjsontest.cpp"
   )
if(Boost_FOUND)
    list(APPEND nativejson_adapters "${CMAKE_CURRENT_SOURCE_DIR}/boostjsontest.cpp")
endif()

# Include each header ahead of the target's sources: -include for GCC and Clang, /FI for MSVC and clang-cl.
function(nativejson_force_include target)
    foreach(header IN LISTS ARGN)
        if(MSVC)
            target_compile_options(${target} PRIVATE "/FI${header}")
        else()
            target_compile_options(${target} PRIVATE "SHELL:-include ${header}")
        endif()
    endforeach()
endfunction()

# memorystat.h #defines malloc, calloc, realloc and free, then includes <stdlib.h> and <string.h>, whose C++ wrappers
# in libstdc++ now redeclare those names through using-declarations that the macros rewrite. Included first, the
# wrappers are already done by the time the macros exist. Elsewhere it costs nothing.
set(nativejson_preinclude stdlib.h string.h)

function(nativejson_add_libraries target)
    target_include_directories(${target}
                               PRIVATE
                                 "${NATIVEJSON_SRC}"
                                 "${NATIVEJSON_GENERATED}"
                                 "${rapidjson_SOURCE_DIR}/include"
                                 "${nlohmann_json_SOURCE_DIR}/include"
                                 "${simdjson_SOURCE_DIR}/singleheader"
                              )
    if(Boost_FOUND)
        target_link_libraries(${target} PRIVATE Boost::headers)
    endif()
    target_compile_definitions(${target} PRIVATE __STDC_FORMAT_MACROS=1)
    nativejson_force_include(${target} ${nativejson_preinclude})
endfunction()

# The harness counts C allocations by rewriting malloc, realloc and free with macros in every file that includes
# memorystat.h, which the header-only libraries are compiled into. jsonv is a library of its own, and parse_index
# allocates its tape with std::malloc, so jsonv gets the header too; otherwise the tape would be missing from its
# memory columns.
target_include_directories(jsonv PRIVATE "${NATIVEJSON_SRC}" "${NATIVEJSON_GENERATED}")
nativejson_force_include(jsonv ${nativejson_preinclude} memorystat.h)

# BenchCodeSize looks for the jsonstat programs in a jsonstat/ directory beside the running program. A multi-config
# generator would otherwise put the configuration last, as jsonstat/Release, rather than first; a generator
# expression in the output directory stops CMake appending it.
set(nativejson_runtime_dir "${CMAKE_CURRENT_BINARY_DIR}$<$<BOOL:${NATIVEJSON_MULTI_CONFIG}>:/$<CONFIG>>")

# The adapters register themselves through static constructors, so they are linked as objects, not from a library.
add_executable(nativejson-benchmark
               "${NATIVEJSON_GENERATED}/main.cpp"
               "${NATIVEJSON_SRC}/memorystat.cpp"
               ${nativejson_adapters}
              )
nativejson_add_libraries(nativejson-benchmark)
target_link_libraries(nativejson-benchmark PRIVATE jsonv::jsonv)
set_target_properties(nativejson-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${nativejson_runtime_dir}")

# One program per library which only parses and counts, whose file size is the "7. Code size" row. The definitions
# are upstream's jsonstat solution's.
foreach(adapter IN LISTS nativejson_adapters)
    get_filename_component(adapter_name "${adapter}" NAME_WE)
    set(target "jsonstat_${adapter_name}")
    add_executable(${target} "${NATIVEJSON_SRC}/jsonstat/jsonstatmain.cpp" "${adapter}")
    nativejson_add_libraries(${target})
    target_compile_definitions(${target}
                               PRIVATE
                                 USE_MEMORYSTAT=0
                                 TEST_PARSE=1
                                 TEST_STRINGIFY=0
                                 TEST_PRETTIFY=0
                                 TEST_STATISTICS=1
                                 TEST_SAXROUNDTRIP=0
                                 TEST_SAXSTATISTICS=0
                                 TEST_SAXSTATISTICSUTF16=0
                                 TEST_CONFORMANCE=0
                                 TEST_INFO=0
                              )
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${nativejson_runtime_dir}/jsonstat")
endforeach()

# jsonv's calls to the MemoryStat functions still need a definition where memorystat.cpp is not linked.
target_sources(jsonstat_jsonvtest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/memorystat_passthrough.cpp")
target_link_libraries(jsonstat_jsonvtest PRIVATE jsonv::jsonv)

################################################################################
# Tests                                                                        #
################################################################################

# --verify-only checks that every library parses each data file to the reference statistics and survives a stringify
# round trip. The program reports problems on stdout and exits 0 regardless.
enable_testing()
add_test(NAME nativejson-verify
         COMMAND nativejson-benchmark --verify-only
         WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/run"
        )
set_tests_properties(nativejson-verify PROPERTIES FAIL_REGULAR_EXPRESSION "Failed|Error:|Cannot read")
