# CPack configuration. Only ever included by a top-level build: include(CPack) writes CPackConfig.cmake into the
# build root and adds a `package` target, neither of which belongs in a project that pulled jsonv in as a dependency.
if(NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32 OR APPLE))
    return()
endif()

include(CPackComponent)

set(JSONV_SO_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

set(CPACK_COMPONENTS_ALL runtime development)
set(CPACK_COMPONENTS_GROUPING IGNORE)
set(CPACK_PACKAGE_NAME "jsonv")
set(CPACK_PACKAGE_VENDOR "Travis Gockel")
set(CPACK_PACKAGE_CONTACT "Travis Gockel <travis@gockelhut.com>")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION "JSON Voorhees is a modern C++ library for interacting with JSON.")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_VERBATIM_VARIABLES YES)

cpack_add_component(runtime
                    DISPLAY_NAME "JSON Voorhees runtime"
                    DESCRIPTION "Shared library for JSON Voorhees."
                   )
cpack_add_component(development
                    DISPLAY_NAME "JSON Voorhees development files"
                    DESCRIPTION "Headers and build metadata for compiling against JSON Voorhees."
                    DEPENDS runtime
                   )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB;RPM"
        CACHE STRING "CPack generators to use for binary packages."
       )
    # libjsonv.pc bakes CMAKE_INSTALL_PREFIX in, so the two must agree.
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
    set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY ">=")
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "libjsonv${JSONV_SO_VERSION}")
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_SECTION libs)
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_RUNTIME_DESCRIPTION "Killer modern C++ library for interacting with JSON.")
    set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_NAME "libjsonv-dev")
    set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_SECTION libdevel)
    set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_DEPENDS
        "libjsonv${JSONV_SO_VERSION} (= ${PROJECT_VERSION}-${CPACK_DEBIAN_PACKAGE_RELEASE})"
       )
    set(CPACK_DEBIAN_DEVELOPMENT_DESCRIPTION "Development files for JSON Voorhees.")

    set(CPACK_RPM_COMPONENT_INSTALL ON)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_RELEASE 1)
    set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
    set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
    set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_RPM_MAIN_COMPONENT runtime)
    set(CPACK_RPM_RUNTIME_PACKAGE_NAME "jsonv")
    set(CPACK_RPM_RUNTIME_PACKAGE_SUMMARY "Killer JSON for C++")
    set(CPACK_RPM_RUNTIME_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
    set(CPACK_RPM_DEVELOPMENT_PACKAGE_NAME "jsonv-devel")
    set(CPACK_RPM_DEVELOPMENT_PACKAGE_SUMMARY "Development files for JSON Voorhees")
    set(CPACK_RPM_DEVELOPMENT_PACKAGE_DESCRIPTION "Headers and build metadata for compiling against JSON Voorhees.")
    set(CPACK_RPM_DEVELOPMENT_PACKAGE_REQUIRES "jsonv = ${PROJECT_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}")
elseif(WIN32)
    set(CPACK_GENERATOR "NuGet"
        CACHE STRING "CPack generators to use for binary packages."
       )
    # Empty so that the package root is the prefix: that is what makes jsonv-config.cmake's relative
    # _IMPORT_PREFIX resolve to wherever NuGet unpacked the package.
    set(CPACK_PACKAGING_INSTALL_PREFIX "")
    set(CPACK_NUGET_PACKAGE_NAME "jsonv")
    set(CPACK_NUGET_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_NUGET_PACKAGE_TITLE "JSON Voorhees")
    set(CPACK_NUGET_PACKAGE_AUTHORS "Travis Gockel")
    set(CPACK_NUGET_PACKAGE_DESCRIPTION "${CPACK_PACKAGE_DESCRIPTION}")
    set(CPACK_NUGET_PACKAGE_HOMEPAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
    set(CPACK_NUGET_PACKAGE_LICENSE_EXPRESSION "Apache-2.0")
    set(CPACK_NUGET_PACKAGE_TAGS "native C++ JSON jsonv")
    set(CPACK_NUGET_PACKAGE_REPOSITORY_URL
        "https://github.com/tgockel/json-voorhees.git"
       )
    set(CPACK_NUGET_PACKAGE_REPOSITORY_TYPE git)
elseif(APPLE)
    set(CPACK_GENERATOR "productbuild"
        CACHE STRING "CPack generators to use for binary packages."
       )
    set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
    configure_file("${PROJECT_SOURCE_DIR}/LICENSE"
                   "${PROJECT_BINARY_DIR}/CPackLicense.txt"
                   COPYONLY
                  )
    set(CPACK_RESOURCE_FILE_LICENSE
        "${PROJECT_BINARY_DIR}/CPackLicense.txt"
       )
    set(CPACK_PRODUCTBUILD_IDENTIFIER "com.tgockel.jsonv")
    set(CPACK_PRODUCTBUILD_DOMAINS ON)
    set(CPACK_PRODUCTBUILD_DOMAINS_ANYWHERE OFF)
    set(CPACK_PRODUCTBUILD_DOMAINS_USER OFF)
    set(CPACK_PRODUCTBUILD_DOMAINS_ROOT ON)
endif()

include(CPack)
