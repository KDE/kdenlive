# Find the OpenTimelineIO library
#
# SPDX-FileCopyrightText: 2024 Darby Johnston <darbyjohnston@yahoo.com>
# SPDX-License-Identifier: BSD-2-Clause

find_path(IMATH_INCLUDE_DIR NAMES ImathBox.h PATH_SUFFIXES Imath)
set(IMATH_INCLUDE_DIRS ${IMATH_INCLUDE_DIR})

find_path(OTIO_INCLUDE_DIR NAMES opentimelineio/version.h)
set(OTIO_INCLUDE_DIRS ${OTIO_INCLUDE_DIR})

find_library(
    opentime_LIBRARY
    NAMES opentime
    PATHS ${CMAKE_INSTALL_PREFIX}/python/opentimelineio)
find_library(
    opentimelineio_LIBRARY
    NAMES opentimelineio
    PATHS ${CMAKE_INSTALL_PREFIX}/python/opentimelineio)
set(OTIO_LIBRARIES ${opentimelineio_LIBRARY} ${opentime_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    OTIO
    REQUIRED_VARS IMATH_INCLUDE_DIR OTIO_INCLUDE_DIR opentimelineio_LIBRARY opentime_LIBRARY)
mark_as_advanced(IMATH_INCLUDE_DIR OTIO_INCLUDE_DIR opentimelineio_LIBRARY opentime_LIBRARY)

if(OTIO_FOUND AND NOT TARGET OTIO::opentime)
    add_library(OTIO::opentime UNKNOWN IMPORTED)
    set_target_properties(OTIO::opentime PROPERTIES
        IMPORTED_LOCATION "${opentime_LIBRARY}"
        INTERFACE_COMPILE_DEFINITIONS opentime_FOUND
        INTERFACE_INCLUDE_DIRECTORIES "${opentime_INCLUDE_DIR}")
endif()
if(OTIO_FOUND AND NOT TARGET OTIO::opentimelineio)
    add_library(OTIO::opentimelineio UNKNOWN IMPORTED)
    set_target_properties(OTIO::opentimelineio PROPERTIES
        IMPORTED_LOCATION "${opentimelineio_LIBRARY}"
        INTERFACE_COMPILE_DEFINITIONS opentimelineio_FOUND
        INTERFACE_INCLUDE_DIRECTORIES "${IMATH_INCLUDE_DIR};${OTIO_INCLUDE_DIR}")
endif()
if(OTIO_FOUND AND NOT TARGET OTIO)
    add_library(OTIO INTERFACE)
    target_link_libraries(OTIO INTERFACE OTIO::opentimelineio OTIO::opentime)
endif()
