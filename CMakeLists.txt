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
cmake_minimum_required(VERSION 3.12...3.29)
project(json-voorhees)

set(PACKAGE_NAME "jsonv"
    CACHE STRING "The packaging name of this library."
   )

################################################################################
# Configuration                                                                #
################################################################################

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
  message(STATUS "No build type selected, default to ${CMAKE_BUILD_TYPE}")
endif()
message(STATUS "Configuration: ${CMAKE_BUILD_TYPE}")

# Extract version information from the config file. It can still be overridden by the user setting JSONV_VERSION, but
# that is probably a bad idea.
file(READ include/jsonv/config.hpp JSONV_CONFIG_H_STR)
string(REGEX REPLACE ".*#define +JSONV_VERSION_MAJOR +([0-9]+).*" "\\1" JSONV_VERSION_MAJOR "${JSONV_CONFIG_H_STR}")
string(REGEX REPLACE ".*#define +JSONV_VERSION_MINOR +([0-9]+).*" "\\1" JSONV_VERSION_MINOR "${JSONV_CONFIG_H_STR}")
string(REGEX REPLACE ".*#define +JSONV_VERSION_PATCH +([0-9]+).*" "\\1" JSONV_VERSION_PATCH "${JSONV_CONFIG_H_STR}")

set(JSONV_VERSION    "${JSONV_VERSION_MAJOR}.${JSONV_VERSION_MINOR}.${JSONV_VERSION_PATCH}")
set(JSONV_SO_VERSION "${JSONV_VERSION_MAJOR}.${JSONV_VERSION_MINOR}")
message(STATUS "JSONV version: ${JSONV_VERSION} (${JSONV_SO_VERSION})")

set(CXX_STANDARD_DEFAULT "c++17")
set(CXX_STANDARD ${CXX_STANDARD_DEFAULT}
    CACHE STRING "The version of the language to use (only applies to GCC and Clang)."
   )

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

if(WIN32)
else(WIN32)
    # Reasonable compilers...
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=${CXX_STANDARD} -pedantic -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -ggdb -DJSONV_DEBUG=1")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ggdb")
endif(WIN32)

include_directories("${PROJECT_SOURCE_DIR}/include"
                    "${PROJECT_SOURCE_DIR}/src"
                   )

add_definitions("-DJSONV_TEST_DATA_DIR=\"${CMAKE_SOURCE_DIR}/src/jsonv-tests/data\"")

include(GNUInstallDirs)

configure_file(libjsonv.pc.in libjsonv.pc @ONLY)

################################################################################
# Building                                                                     #
################################################################################

file(GLOB_RECURSE jsonv_cpps RELATIVE_PATH "." "src/jsonv/*.cpp")
add_library(jsonv ${LIBRARY_TYPE} ${jsonv_cpps})
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

################################################################################
# Packaging                                                                    #
################################################################################

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    include(CPackComponent)

    set(CPACK_GENERATOR "DEB;RPM"
        CACHE STRING "CPack generators to use for binary packages."
       )
    set(CPACK_COMPONENTS_ALL runtime development)
    set(CPACK_COMPONENTS_GROUPING IGNORE)
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
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

    include(CPack)
endif()
