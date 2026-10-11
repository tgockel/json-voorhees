# The json-benchmark program. Each third-party suite is optional and found through the system rather than by a
# hard-coded path.
option(JSONV_BENCHMARK_JSONV   "Benchmark suite for JSON Voorhees."                        ON)
option(JSONV_BENCHMARK_JSONCPP "Benchmark suite for JsonCpp, found through pkg-config."     ON)
option(JSONV_BENCHMARK_JQ      "Benchmark suite for jq."                                   OFF)
option(JSONV_BENCHMARK_JANSSON "Benchmark suite for jansson, found through pkg-config."     OFF)

add_executable(json-benchmark core.cpp core.hpp main.cpp)
# main.cpp includes <jsonv/all.hpp> whichever suites are on.
target_link_libraries(json-benchmark PRIVATE jsonv::jsonv)
jsonv_apply_dev_flags(json-benchmark)

if(JSONV_BENCHMARK_JSONV)
    target_sources(json-benchmark PRIVATE jsonv_benchmark.cpp)
endif()

if(JSONV_BENCHMARK_JSONCPP OR JSONV_BENCHMARK_JANSSON)
    find_package(PkgConfig REQUIRED)
endif()

if(JSONV_BENCHMARK_JSONCPP)
    pkg_check_modules(jsoncpp REQUIRED IMPORTED_TARGET jsoncpp)
    target_sources(json-benchmark PRIVATE jsoncpp_benchmark.cpp)
    target_link_libraries(json-benchmark PRIVATE PkgConfig::jsoncpp)
endif()

if(JSONV_BENCHMARK_JANSSON)
    pkg_check_modules(jansson REQUIRED IMPORTED_TARGET jansson)
    target_sources(json-benchmark PRIVATE jansson_benchmark.cpp)
    target_link_libraries(json-benchmark PRIVATE PkgConfig::jansson)
endif()

if(JSONV_BENCHMARK_JQ)
    find_library(JSONV_JQ_LIBRARY NAMES jq REQUIRED)
    find_path(JSONV_JQ_INCLUDE_DIR NAMES jv.h REQUIRED)
    mark_as_advanced(JSONV_JQ_LIBRARY JSONV_JQ_INCLUDE_DIR)
    target_sources(json-benchmark PRIVATE jq_benchmark.cpp)
    target_include_directories(json-benchmark PRIVATE "${JSONV_JQ_INCLUDE_DIR}")
    target_link_libraries(json-benchmark PRIVATE "${JSONV_JQ_LIBRARY}")
endif()
