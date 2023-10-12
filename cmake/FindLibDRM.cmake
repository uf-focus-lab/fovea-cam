#.rst:
# FindLibDRM
# -------
#
# Try to find libdrm on a Unix system.
#
# This will define the following variables:
#
# ``LibDRM_FOUND``
#     True if (the requested version of) libdrm is available
# ``LibDRM_VERSION``
#     The version of libdrm
# ``LibDRM_LIBRARIES``
#     This can be passed to target_link_libraries() instead of the ``LibDRM::LibDRM``
#     target
# ``LibDRM_INCLUDE_DIRS``
#     This should be passed to target_include_directories() if the target is not
#     used for linking
# ``LibDRM_DEFINITIONS``
#     This should be passed to target_compile_options() if the target is not
#     used for linking
#
# If ``LibDRM_FOUND`` is TRUE, it will also define the following imported target:
#
# ``LibDRM::LibDRM``
#     The libdrm library
#
# In general we recommend using the imported target, as it is easier to use.
# Bear in mind, however, that if the target is in the link interface of an
# exported library, it must be made available by the package config file.

#=============================================================================
# SPDX-FileCopyrightText: 2014 Alex Merry <alex.merry@kde.org>
# SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
#
# SPDX-License-Identifier: BSD-3-Clause
#=============================================================================

if(CMAKE_VERSION VERSION_LESS 2.8.12)
    message(FATAL_ERROR "CMake 2.8.12 is required by FindLibDRM.cmake")
endif()
if(CMAKE_MINIMUM_REQUIRED_VERSION VERSION_LESS 2.8.12)
    message(AUTHOR_WARNING "Your project should require at least CMake 2.8.12 to use FindLibDRM.cmake")
endif()

if(NOT WIN32)
    # Use pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    find_package(PkgConfig)
    pkg_check_modules(PKG_LibDRM QUIET libdrm)

    set(LibDRM_DEFINITIONS ${PKG_LibDRM_CFLAGS_OTHER})
    set(LibDRM_VERSION ${PKG_LibDRM_VERSION})

    find_path(LibDRM_INCLUDE_DIR
        NAMES
            xf86drm.h
        HINTS
            ${PKG_LibDRM_INCLUDE_DIRS}
    )
    find_library(LibDRM_LIBRARY
        NAMES
            drm
        HINTS
            ${PKG_LibDRM_LIBRARY_DIRS}
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(LibDRM
        FOUND_VAR
            LibDRM_FOUND
        REQUIRED_VARS
            LibDRM_LIBRARY
            LibDRM_INCLUDE_DIR
        VERSION_VAR
            LibDRM_VERSION
    )

    if(LibDRM_FOUND AND NOT TARGET LibDRM::LibDRM)
        add_library(LibDRM::LibDRM UNKNOWN IMPORTED)
        set_target_properties(LibDRM::LibDRM PROPERTIES
            IMPORTED_LOCATION "${LibDRM_LIBRARY}"
            INTERFACE_COMPILE_OPTIONS "${LibDRM_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES "${LibDRM_INCLUDE_DIR}"
            INTERFACE_INCLUDE_DIRECTORIES "${LibDRM_INCLUDE_DIR}/libdrm"
        )
    endif()

    mark_as_advanced(LibDRM_LIBRARY LibDRM_INCLUDE_DIR)

    # compatibility variables
    set(LibDRM_LIBRARIES ${LibDRM_LIBRARY})
    set(LibDRM_INCLUDE_DIRS ${LibDRM_INCLUDE_DIR} "${LibDRM_INCLUDE_DIR}/libdrm")
    set(LibDRM_VERSION_STRING ${LibDRM_VERSION})

else()
    message(STATUS "FindLibDRM.cmake cannot find libdrm on Windows systems.")
    set(LibDRM_FOUND FALSE)
endif()

include(FeatureSummary)
set_package_properties(LibDRM PROPERTIES
    URL "https://wiki.freedesktop.org/dri/"
    DESCRIPTION "Userspace interface to kernel DRM services"
)
