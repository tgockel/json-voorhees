# The unit test suite: one executable, registered with CTest as one test.
set(jsonv_tests_sources
    algorithm_compare_tests.cpp
    algorithm_diff_tests.cpp
    algorithm_map_tests.cpp
    algorithm_merge_tests.cpp
    algorithm_validate_tests.cpp
    allocation_counter.cpp
    allocation_counter.hpp
    array_tests.cpp
    benchmark_tests.cpp
    builtin_formats_tests.cpp
    char_convert_tests.cpp
    chrono_io.cpp
    chrono_io.hpp
    coerce_tests.cpp
    composite_adapter_tests.cpp
    config_tests.cpp
    demangle_tests.cpp
    deserialize_tests.cpp
    documentation_tests.cpp
    encode_tests.cpp
    enum_adapter_tests.cpp
    filesystem_util.cpp
    filesystem_util.hpp
    fixed_map_tests.cpp
    functional_tests.cpp
    json_checker.cpp
    kind_tests.cpp
    locale_util.hpp
    main.cpp
    object_tests.cpp
    optional_tests.cpp
    parse_blns.cpp
    parse_index_tests.cpp
    parse_tests.cpp
    path_tests.cpp
    reader_tests.cpp
    serialization_builder_tests.cpp
    serialization_tests.cpp
    serialize_tests.cpp
    stopwatch.cpp
    stopwatch.hpp
    string_tests.cpp
    test.cpp
    test.hpp
    unicode_tests.cpp
    value_encoder_tests.cpp
    value_tests.cpp
    writer_tests.cpp
   )

add_executable(jsonv-tests ${jsonv_tests_sources})
target_link_libraries(jsonv-tests PRIVATE jsonv::jsonv)

# For `<jsonv-tests/test.hpp>` and the private `<jsonv/detail.hpp>`, `<jsonv/char_convert.hpp>` and
# `<jsonv/detail/fixed_map.hpp>` which the white-box tests reach into.
target_include_directories(jsonv-tests PRIVATE "${PROJECT_SOURCE_DIR}/src")

target_compile_definitions(jsonv-tests
                           PRIVATE
                             "JSONV_TEST_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/data\""
                             # The version `cmake/JsonvVersion.cmake` read, for `config_tests.cpp` to hold against what
                             # the compiler reads from the same header.
                             "JSONV_TEST_VERSION=\"${JSONV_VERSION_SEMVER}\""
                             # `JSONV_DEBUG` throttles the timing loops: without it `benchmark_tests.cpp` runs 100
                             # iterations over the multi-megabyte corpus and `deserialize_tests.cpp` sizes its
                             # linearity guard at 20000 elements. Only the tests read it -- not the library, not the
                             # public headers -- so it is theirs alone and stays out of anything installed. A
                             # generator expression so that multi-config generators get the right answer.
                             "$<$<CONFIG:Debug>:JSONV_DEBUG=1>"
                          )

# `allocation_counter` replaces the global `operator new` and `operator delete`, which is not something to do to
# AddressSanitizer -- it installs its own, and taking them over costs the whole suite its `alloc-dealloc-mismatch`
# detection. So the counter compiles itself out under a sanitizer, and the tests which assert what path tracking
# costs go with it. That leaves the failure mode where the detection misfires somewhere it should not have, and those
# tests quietly stop existing in a run that stays green. This is the second opinion: CMake knows whether sanitizers
# were asked for, so it is the one thing in the build that can tell the counter it is wrong.
if(NOT JSONV_SANITIZE)
    target_compile_definitions(jsonv-tests PRIVATE JSONV_TEST_REQUIRE_ALLOCATION_COUNTING=1)
endif()

# Windows gives a thread 1 MB of stack where Linux and macOS give 8. `value` is a tree and its destructor recurses
# once per level, so tearing down the 4096-deep document `reader_from_value_deep_nesting` builds exhausts that in an
# unoptimized build -- the walk under test is a loop and is not what runs out. Nothing a document can do reaches this
# depth: `parse` refuses anything past `parse_options::k::max_structure_depth`, which is 128 and cannot be raised
# past its own ceiling, so only a value built in code gets this deep.
if(MSVC)
    target_link_options(jsonv-tests PRIVATE /STACK:8388608)
endif()

jsonv_apply_dev_flags(jsonv-tests)

# The runner finds its data through the absolute JSONV_TEST_DATA_DIR, so the working directory only decides where a
# relative path somebody adds later would land.
add_test(NAME jsonv-tests
         COMMAND jsonv-tests
         WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        )
set_tests_properties(jsonv-tests PROPERTIES TIMEOUT 1800)

get_target_property(jsonv_type jsonv TYPE)
if(WIN32 AND jsonv_type STREQUAL "SHARED_LIBRARY")
    # A DLL build has to be on PATH for the executable to start. The top-level build puts jsonv.dll next to the
    # executable; a build where this is a subproject does not.
    set_tests_properties(jsonv-tests
                         PROPERTIES
                           ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:jsonv>"
                        )
endif()

# Convenience for the documented `cmake --build build --target check`. Guarded because a parent project which
# switched JSONV_BUILD_TESTS on may well have a `check` of its own.
if(NOT TARGET check)
    add_custom_target(check
                      COMMAND $<TARGET_FILE:jsonv-tests>
                      DEPENDS jsonv-tests
                      USES_TERMINAL
                     )
endif()
