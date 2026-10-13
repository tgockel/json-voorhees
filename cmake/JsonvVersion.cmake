# Reads the library version out of config.hpp, which is the one place it is written down, and sets in the caller:
#
#   <prefix>             2.0.0       The three numbers. project(VERSION), the shared library's VERSION and SOVERSION and
#                                    the CMake package version file take nothing else.
#   <prefix>_PRERELEASE  rc.1        JSONV_VERSION_PRERELEASE; empty for a release.
#   <prefix>_SEMVER      2.0.0-rc.1  As SemVer and the git tag spell it: NuGet, the macOS installer, CPack file names.
#   <prefix>_TILDE       2.0.0~rc.1  '~' for '-'. dpkg, rpm and pkgconf sort this before 2.0.0 and the SemVer spelling
#                                    after it, so the deb and rpm versions and libjsonv.pc use this one.
#
# For a release, the last two are the same as the first.
#
# A failed match is an error. The previous `string(REGEX REPLACE ".*#define ..." ...)` handed the whole header back
# as the version when its pattern did not match, and nothing downstream noticed.
function(jsonv_read_version header prefix)
    file(READ "${header}" contents)
    foreach(part IN ITEMS MAJOR MINOR PATCH)
        if(NOT contents MATCHES "#define[ \t]+JSONV_VERSION_${part}[ \t]+([0-9]+)")
            message(FATAL_ERROR "JSONV_VERSION_${part} not found in ${header}")
        endif()
        set(version_${part} "${CMAKE_MATCH_1}")
    endforeach()

    if(NOT contents MATCHES "#define[ \t]+JSONV_VERSION_PRERELEASE[ \t]+\"([^\"]*)\"")
        message(FATAL_ERROR "JSONV_VERSION_PRERELEASE not found in ${header}")
    endif()
    set(prerelease "${CMAKE_MATCH_1}")

    # SemVer's pre-release grammar, less the '-' it allows inside an identifier: rpm refuses a '-' in a version, and
    # CPack would quietly make it a '_', which sorts differently from what dpkg and pkgconf make of the '-'.
    if(NOT prerelease STREQUAL ""
       AND (NOT prerelease MATCHES "^[0-9A-Za-z]+(\\.[0-9A-Za-z]+)*$"
            OR prerelease MATCHES "(^|\\.)0[0-9]+(\\.|$)"))
        message(FATAL_ERROR
                "JSONV_VERSION_PRERELEASE in ${header} is \"${prerelease}\". A pre-release is dot-separated "
                "identifiers of letters and digits, such as \"rc.1\", written without the '-' which joins it to the "
                "version; an identifier which is a number does not start with 0. A release is \"\"."
               )
    endif()

    # file(READ) does not register a configure dependency on its own. Without this a version bump would be picked up
    # by the next configure somebody happened to run by hand, not by the next build.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${header}")

    set(numeric "${version_MAJOR}.${version_MINOR}.${version_PATCH}")
    set(semver  "${numeric}")
    set(tilde   "${numeric}")
    if(NOT prerelease STREQUAL "")
        string(APPEND semver "-${prerelease}")
        string(APPEND tilde  "~${prerelease}")
    endif()

    set(${prefix}            "${numeric}"    PARENT_SCOPE)
    set(${prefix}_PRERELEASE "${prerelease}" PARENT_SCOPE)
    set(${prefix}_SEMVER     "${semver}"     PARENT_SCOPE)
    set(${prefix}_TILDE      "${tilde}"      PARENT_SCOPE)
endfunction()
