# Reads the library version out of config.hpp, which is the one place it is written down.
#
# A failed match is an error. The previous `string(REGEX REPLACE ".*#define ..." ...)` handed the whole header back
# as the version when its pattern did not match, and nothing downstream noticed.
function(jsonv_read_version header out_var)
    file(READ "${header}" contents)
    foreach(part IN ITEMS MAJOR MINOR PATCH)
        if(NOT contents MATCHES "#define[ \t]+JSONV_VERSION_${part}[ \t]+([0-9]+)")
            message(FATAL_ERROR "JSONV_VERSION_${part} not found in ${header}")
        endif()
        set(version_${part} "${CMAKE_MATCH_1}")
    endforeach()

    # file(READ) does not register a configure dependency on its own. Without this a version bump would be picked up
    # by the next configure somebody happened to run by hand, not by the next build.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${header}")

    set(${out_var} "${version_MAJOR}.${version_MINOR}.${version_PATCH}" PARENT_SCOPE)
endfunction()
