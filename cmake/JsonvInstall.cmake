# Install rules and the CMake package: what `find_package(jsonv CONFIG)` finds.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(JSONV_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/jsonv"
    CACHE PATH "Directory, relative to the install prefix, for the jsonv CMake package files."
   )
mark_as_advanced(JSONV_INSTALL_CMAKEDIR)

install(TARGETS jsonv
        EXPORT   jsonv-targets
        RUNTIME  DESTINATION "${CMAKE_INSTALL_BINDIR}"
                 COMPONENT runtime
        LIBRARY  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                 COMPONENT runtime
                 NAMELINK_COMPONENT development
        ARCHIVE  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
                 COMPONENT development
        # include/jsonv/..., with the structure under include/ preserved.
        FILE_SET HEADERS
                 DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
                 COMPONENT development
       )

install(EXPORT jsonv-targets
        NAMESPACE   jsonv::
        FILE        jsonv-targets.cmake
        DESTINATION "${JSONV_INSTALL_CMAKEDIR}"
        COMPONENT   development
       )

configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/jsonv-config.cmake.in"
                              "${PROJECT_BINARY_DIR}/cmake/jsonv-config.cmake"
                              INSTALL_DESTINATION "${JSONV_INSTALL_CMAKEDIR}"
                              NO_SET_AND_CHECK_MACRO
                             )
# PROJECT_VERSION has no pre-release, so find_package(jsonv 2.0) accepts the release candidates of 2.0.0, just as
# `#if JSONV_VERSION >= 2000000` does.
write_basic_package_version_file("${PROJECT_BINARY_DIR}/cmake/jsonv-config-version.cmake"
                                 VERSION       "${PROJECT_VERSION}"
                                 COMPATIBILITY SameMajorVersion
                                )
install(FILES "${PROJECT_BINARY_DIR}/cmake/jsonv-config.cmake"
              "${PROJECT_BINARY_DIR}/cmake/jsonv-config-version.cmake"
        DESTINATION "${JSONV_INSTALL_CMAKEDIR}"
        COMPONENT   development
       )

# pkg-config and MSBuild consumers never see INTERFACE_COMPILE_DEFINITIONS, so the one definition a static build
# exports has to be written into those files by hand.
get_target_property(jsonv_type jsonv TYPE)
if(jsonv_type STREQUAL "STATIC_LIBRARY")
    set(JSONV_PC_CFLAGS       " -DJSONV_SO=0")
    set(JSONV_MSBUILD_DEFINES "JSONV_SO=0;")
else()
    set(JSONV_PC_CFLAGS       "")
    set(JSONV_MSBUILD_DEFINES "")
endif()

# The .pc file locates the prefix from its own position -- `${pcfiledir}` is the directory pkg-config read it from
# -- so the install stays usable when `cmake --install --prefix` puts it somewhere other than CMAKE_INSTALL_PREFIX,
# or the whole tree is moved. The CMake package files already work this way. A destination given as an absolute
# path cannot move with the prefix, so one of those is written as it is.
set(JSONV_PC_PKGCONFIGDIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(JSONV_PC_PREFIX "${CMAKE_INSTALL_PREFIX}")
    set(JSONV_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
    file(RELATIVE_PATH jsonv_pc_to_prefix "${CMAKE_INSTALL_PREFIX}/${JSONV_PC_PKGCONFIGDIR}" "${CMAKE_INSTALL_PREFIX}")
    string(REGEX REPLACE "/+$" "" jsonv_pc_to_prefix "${jsonv_pc_to_prefix}")
    set(JSONV_PC_PREFIX "\${pcfiledir}/${jsonv_pc_to_prefix}")
    set(JSONV_PC_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(JSONV_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(JSONV_PC_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

# The version is written as JSONV_VERSION_TILDE, so that pkgconf sorts a pre-release before its release as dpkg and
# rpm do with the packages: `libjsonv >= 2.0.0` is not met by 2.0.0~rc.1, and `libjsonv >= 2.0.0~rc.1` is met by 2.0.0.
configure_file("${PROJECT_SOURCE_DIR}/libjsonv.pc.in" "${PROJECT_BINARY_DIR}/libjsonv.pc" @ONLY)
install(FILES "${PROJECT_BINARY_DIR}/libjsonv.pc"
        DESTINATION "${JSONV_PC_PKGCONFIGDIR}"
        COMPONENT   development
       )

if(WIN32)
    configure_file("${PROJECT_SOURCE_DIR}/packaging/nuget/jsonv.targets.in"
                   "${PROJECT_BINARY_DIR}/nuget/jsonv.targets"
                   @ONLY
                  )
    install(FILES "${PROJECT_BINARY_DIR}/nuget/jsonv.targets"
            DESTINATION "build/native"
            COMPONENT   development
           )
endif()
