# File: CMakeLists used for generating a Makefile (or whatever your build system is).
#
# Copyright 2015 by Travis Gockel
# 
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
# an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.
cmake_minimum_required(VERSION 2.8)
project(json-voorhees)

################################################################################
# Configuration                                                                #
################################################################################

set(JSONV_VERSION "1.0.0-pre0"
    CACHE STRING "The version of the library."
   )

set(REQUIRED_BOOST_LIBRARIES "filesystem" "system")

option(USE_BOOST_REGEX
       "Controls the variable JSONV_REGEX_USE_BOOST (see C++ documentation)."
       OFF
      )
if (USE_BOOST_REGEX)
    add_definitions("-DJSONV_REGEX_USE_BOOST=1")
    set(REQUIRED_BOOST_LIBRARIES "regex" ${REQUIRED_BOOST_LIBRARIES})
endif(USE_BOOST_REGEX)

option(USE_BOOST_STRING_VIEW
       "Controls the variable JSONV_STRING_VIEW_USE_BOOST (see C++ documentation)."
       OFF
      )
if (USE_BOOST_STRING_VIEW)
    add_definitions("-DJSONV_STRING_VIEW_USE_BOOST=1")
endif(USE_BOOST_STRING_VIEW)

option(USE_STD_STRING_VIEW
       "Controls the variable JSONV_STRING_VIEW_USE_STD (see C++ documentation)."
       OFF
      )
if (USE_STD_STRING_VIEW)
    add_definitions("-DJSONV_STRING_VIEW_USE_STD=1")
endif(USE_STD_STRING_VIEW)

option(COVERAGE
       "Enable building with code coverage."
       OFF
      )
if (COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif(COVERAGE)

option(BENCHMARK
       "Enable compilation of the json-benchmark program."
       OFF
      )

if(WIN32)
else(WIN32)
    # Reasonable compilers...
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++11 -pedantic -Wall -Wextra -ggdb")
endif(WIN32)

include_directories("${PROJECT_SOURCE_DIR}/include"
                    "${PROJECT_SOURCE_DIR}/src"
                   )

if (REQUIRED_BOOST_LIBRARIES)
    set(Boost_USE_MULTITHREADED ON)
    find_package(Boost
                 COMPONENTS ${REQUIRED_BOOST_LIBRARIES}
                 REQUIRED
                )
endif(REQUIRED_BOOST_LIBRARIES)

add_definitions("-DJSONV_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/src/jsonv-tests/data\"")

################################################################################
# Building                                                                     #
################################################################################

file(GLOB_RECURSE jsonv_cpps RELATIVE_PATH "." "src/jsonv/*.cpp")
add_library(jsonv SHARED ${jsonv_cpps})
set_target_properties(jsonv
                      PROPERTIES
                          SOVERSION ${JSONV_VERSION}
                          VERSION   ${JSONV_VERSION}
                     )

file(GLOB_RECURSE jsonv_tests_cpps RELATIVE_PATH "." "src/jsonv-tests/*.cpp")
add_executable(jsonv-tests ${jsonv_tests_cpps})
target_link_libraries(jsonv-tests
        "jsonv"
        ${Boost_LIBRARIES}
    )

if (COVERAGE)
    target_link_libraries(jsonv "gcov")
    target_link_libraries(jsonv-tests "gcov")
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
        ${Boost_LIBRARIES}
    )
endif(BENCHMARK)

################################################################################
# Installation                                                                 #
################################################################################

install(TARGETS jsonv
        LIBRARY DESTINATION lib
       )
install(DIRECTORY "include/jsonv" DESTINATION "include")
