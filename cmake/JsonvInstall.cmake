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

configure_file("${PROJECT_SOURCE_DIR}/libjsonv.pc.in" "${PROJECT_BINARY_DIR}/libjsonv.pc" @ONLY)
install(FILES "${PROJECT_BINARY_DIR}/libjsonv.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
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
