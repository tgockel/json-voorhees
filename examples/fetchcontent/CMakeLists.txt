# A project which depends on JSON Voorhees through FetchContent. This is the snippet from README.md; CI configures it
# against the checkout it lives in and against an installed copy to make sure both keep working.
#
# FIND_PACKAGE_ARGS is CMake 3.24: FetchContent_MakeAvailable tries find_package(jsonv) first and only fetches when
# that fails. Leave the keyword out on 3.23 and the library is always fetched.
cmake_minimum_required(VERSION 3.24)
project(jsonv-example LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(jsonv
                     GIT_REPOSITORY https://github.com/tgockel/json-voorhees.git
                     GIT_TAG        v2.0.0
                     FIND_PACKAGE_ARGS
                    )
FetchContent_MakeAvailable(jsonv)

add_executable(jsonv-example main.cpp)
target_link_libraries(jsonv-example PRIVATE jsonv::jsonv)
