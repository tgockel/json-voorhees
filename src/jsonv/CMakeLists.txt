# The library.
#
# Sources and headers are listed rather than globbed: adding a file is a visible change, and a glob which happens to
# be stale cannot silently leave one out. src/jsonv-fuzz reads this target's SOURCES to build its instrumented copy,
# so the .cpp list is the one statement of what the library is made of.
set(jsonv_sources
    algorithm_compare.cpp
    algorithm_diff.cpp
    algorithm_map.cpp
    algorithm_merge.cpp
    algorithm_traverse.cpp
    algorithm_validate.cpp
    array_impl.cpp
    ast.cpp
    char_convert.cpp
    coerce.cpp
    demangle.cpp
    detail.cpp
    detail/match/number.cpp
    detail/match/string.cpp
    encode.cpp
    functional.cpp
    kind.cpp
    object_impl.cpp
    parse.cpp
    parse_index.cpp
    path.cpp
    reader.cpp
    reader_impl.cpp
    reader_impl_parse_index.cpp
    reader_impl_value.cpp
    serialization.cpp
    serialization/builtin_formats.cpp
    serialization/deserialize.cpp
    serialization/formats.cpp
    serialization/serialization_error.cpp
    serialization/serialize.cpp
    serialization_builder.cpp
    string_impl.cpp
    value.cpp
    version.cpp
    writer.cpp
   )

# Not installed. Listed so that IDEs show them with the target.
set(jsonv_private_headers
    array_impl.hpp
    char_convert.hpp
    detail.hpp
    detail/architecture.hpp
    detail/attributes.hpp
    detail/cloneable.hpp
    detail/counted_block.hpp
    detail/fallthrough.hpp
    detail/fast_float/fast_float.h
    detail/fixed_map.hpp
    detail/integer_sum.hpp
    detail/is_print.hpp
    detail/match/number.hpp
    detail/match/string.hpp
    detail/overload.hpp
    detail/utf8.hpp
    object_impl.hpp
    reader_impl.hpp
    reader_impl_parse_index.hpp
    reader_impl_value.hpp
    serialization/describe.hpp
    string_impl.hpp
    value_access.hpp
   )

# Installed to ${CMAKE_INSTALL_INCLUDEDIR}/jsonv/... with this structure preserved.
set(jsonv_public_headers
    algorithm.hpp
    all.hpp
    ast.hpp
    coerce.hpp
    config.hpp
    demangle.hpp
    detail/basic_view.hpp
    detail/generic_container.hpp
    detail/reserve.hpp
    detail/scope_exit.hpp
    encode.hpp
    forward.hpp
    functional.hpp
    kind.hpp
    optional.hpp
    parse.hpp
    parse_index.hpp
    path.hpp
    reader.hpp
    serialization.hpp
    serialization/adapter.hpp
    serialization/adapter_for.hpp
    serialization/all.hpp
    serialization/container_adapter.hpp
    serialization/context.hpp
    serialization/deserialize.hpp
    serialization/deserializer_construction.hpp
    serialization/deserializer_for.hpp
    serialization/enum_adapter.hpp
    serialization/formats.hpp
    serialization/function_adapter.hpp
    serialization/function_deserializer.hpp
    serialization/function_serializer.hpp
    serialization/optional_adapter.hpp
    serialization/polymorphic_adapter.hpp
    serialization/serialization_error.hpp
    serialization/serialize.hpp
    serialization/serializer.hpp
    serialization/serializer_for.hpp
    serialization/wrapper_adapter.hpp
    serialization_builder.hpp
    value.hpp
    version.hpp
    writer.hpp
   )
list(TRANSFORM jsonv_public_headers PREPEND "${PROJECT_SOURCE_DIR}/include/jsonv/")

# STATIC or SHARED per BUILD_SHARED_LIBS. The alias is the name consumers use, so a project which pulled this in
# with add_subdirectory or FetchContent links the same `jsonv::jsonv` as one which found it installed.
add_library(jsonv)
add_library(jsonv::jsonv ALIAS jsonv)

target_sources(jsonv
               PRIVATE
                 ${jsonv_sources}
                 ${jsonv_private_headers}
               PUBLIC
                 FILE_SET HEADERS
                   BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
                   FILES     ${jsonv_public_headers}
              )

# The header file set already contributes $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> here and the install
# destination to the exported target, so only the private tree needs naming.
target_include_directories(jsonv PRIVATE "${PROJECT_SOURCE_DIR}/src")

target_compile_features(jsonv PUBLIC cxx_std_23)

# config.hpp keys JSONV_PUBLIC off JSONV_COMPILING and JSONV_SO. JSONV_COMPILING used to come only from CMake's
# DEFINE_SYMBOL (`jsonv_EXPORTS`), which exists for a shared library alone; saying it outright makes the header's
# export semantics independent of the target's name and type.
target_compile_definitions(jsonv PRIVATE JSONV_COMPILING=1)

get_target_property(jsonv_type jsonv TYPE)
if(jsonv_type STREQUAL "STATIC_LIBRARY")
    # PUBLIC: a usage requirement of the static library, not a detail of compiling it. This is what reaches a target
    # linking jsonv in-tree and the INTERFACE_COMPILE_DEFINITIONS of the installed target; cmake/JsonvInstall.cmake
    # writes the same definition into libjsonv.pc and jsonv.targets. Without it a consumer sees config.hpp's default
    # of JSONV_SO=1 and, on MSVC, compiles every API declaration as __declspec(dllimport).
    target_compile_definitions(jsonv PUBLIC JSONV_SO=0)
endif()

set_target_properties(jsonv
                      PROPERTIES
                        # libjsonv.so.2.0.0 with SONAME libjsonv.so.2.0. ABI compatibility is promised across patch
                        # versions only (see README.md), so the SONAME carries the minor version. A pre-release ships
                        # the same names; only the packages carry the pre-release.
                        VERSION     "${PROJECT_VERSION}"
                        SOVERSION   "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
                        EXPORT_NAME jsonv
                     )

jsonv_apply_dev_flags(jsonv)
