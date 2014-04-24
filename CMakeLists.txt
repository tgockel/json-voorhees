cmake_minimum_required(VERSION 2.8)
project(json-voorhees)

set(CMAKE_CXX_FLAGS "--std=c++11 -fshow-column -pedantic -Werror -Wall -Wextra -Wconversion -Wlogical-op -g -O3")

include_directories(
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/src"
    )

# Build voorhees

file(GLOB_RECURSE voorhees_cpps RELATIVE_PATH "." "src/json-voorhees/*.cpp")
add_library(json-voorhees STATIC ${voorhees_cpps})

# Build the tests

include_directories("${CMAKE_SOURCE_DIR}/tests")

file(GLOB_RECURSE test_cpps RELATIVE_PATH "." "tests/*.cpp")
add_executable(json-voorhees-test ${test_cpps})

target_link_libraries(json-voorhees-test
        "json-voorhees"
        pthread
    )
