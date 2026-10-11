# Compiler and linker flags for this project's own targets.
#
# Everything here is applied PRIVATE to a target this project defines, so none of it reaches the export set,
# libjsonv.pc, jsonv.targets, or a project which pulled jsonv in through add_subdirectory or FetchContent. The one
# exception is a link option on a static library, which has nowhere to go but its consumers; see the function. It is
# a function rather than an INTERFACE target on purpose: an INTERFACE target linked PRIVATE into a static library is
# recorded in that library's INTERFACE_LINK_LIBRARIES as $<LINK_ONLY:...>, and install(EXPORT) then refuses to
# export jsonv because the flags target is not in any export set.

# Shared with src/jsonv-fuzz so the two instrumented builds cannot drift apart. `-fno-sanitize-recover=all` is what
# turns a UBSan report into a non-zero exit instead of a printed warning the run would sail past.
set(JSONV_SANITIZER_FLAGS
    -g
    -fno-omit-frame-pointer
    -fsanitize=address,undefined
    -fno-sanitize-recover=all
   )

if(JSONV_SANITIZE AND MSVC)
    message(FATAL_ERROR "JSONV_SANITIZE requires a GCC- or Clang-style driver.")
endif()

# Link-time optimization gives the compiler a chance to inline across translation units, which matters most for the
# parser hot path (parse_index.cpp + match/string.cpp + match/number.cpp).
set(JSONV_LTO_SUPPORTED OFF)
if(JSONV_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT JSONV_LTO_SUPPORTED OUTPUT JSONV_LTO_CHECK_OUTPUT LANGUAGES CXX)
    if(NOT JSONV_LTO_SUPPORTED)
        message(WARNING "JSONV_LTO requested but the toolchain does not support it: ${JSONV_LTO_CHECK_OUTPUT}")
    endif()
endif()

function(jsonv_apply_dev_flags target)
    if(NOT MSVC)
        target_compile_options(${target} PRIVATE -pedantic -Wall -Wextra "$<$<CONFIG:Debug>:-ggdb>")
        if(JSONV_RELEASE_WITH_DEBUG_SYMBOLS)
            target_compile_options(${target} PRIVATE "$<$<CONFIG:Release>:-ggdb>")
        endif()
    endif()

    # A static archive is never linked, so a PRIVATE link option on one is dropped on the floor, and the executable
    # which eventually links the archive comes up short of the sanitizer or gcov runtime its objects call into. For
    # a static library those options go in the link interface instead, in-tree and in the exported target alike, so
    # a consumer of an instrumented build links the matching runtime without having to know it needed to. A shared
    # library or an executable is linked right here, so for them PRIVATE means what it says.
    get_target_property(target_type ${target} TYPE)
    if(target_type STREQUAL "STATIC_LIBRARY")
        set(link_scope INTERFACE)
    else()
        set(link_scope PRIVATE)
    endif()

    if(JSONV_SANITIZE)
        target_compile_options(${target} PRIVATE ${JSONV_SANITIZER_FLAGS})
        target_link_options(${target} ${link_scope} ${JSONV_SANITIZER_FLAGS})
    endif()

    if(JSONV_COVERAGE)
        # `--coverage` is `-fprofile-arcs -ftest-coverage` when compiling and `-lgcov` when linking, on GCC and Clang.
        target_compile_options(${target} PRIVATE --coverage)
        target_link_options(${target} ${link_scope} --coverage)
    endif()

    if(JSONV_LTO_SUPPORTED)
        # The per-configuration properties rather than the plain INTERPROCEDURAL_OPTIMIZATION, which would apply to
        # every configuration a multi-config generator builds. A Debug build compiled `/GL` and linked `/LTCG` pays
        # the link time and gives up incremental linking to optimize code nobody is measuring.
        set_target_properties(${target}
                              PROPERTIES
                                INTERPROCEDURAL_OPTIMIZATION_RELEASE        TRUE
                                INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
                             )
    endif()
endfunction()
