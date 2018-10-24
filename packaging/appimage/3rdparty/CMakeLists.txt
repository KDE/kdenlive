# Rules to build 3rd-party libraries for digiKam bundles.
#
# Copyright (c) 2015-2017, Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

project(digikam-bundles)

cmake_minimum_required(VERSION 2.8.6)

function(JoinListAsString VALUES GLUE OUTPUT)
    string(REPLACE ";" "${GLUE}" _TMP_STR "${VALUES}")
    set(${OUTPUT} "${_TMP_STR}" PARENT_SCOPE)
endfunction()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Compiling in the source directory is not supported. Use for example 'mkdir build; cd build; cmake ..'.")
endif()

# Tools must be obtained to work with:
include (ExternalProject)

# allow specification of a directory with pre-downloaded
# requirements
if(NOT IS_DIRECTORY ${EXTERNALS_DOWNLOAD_DIR})
    message(FATAL_ERROR "No externals download dir set. Use -DEXTERNALS_DOWNLOAD_DIR")
endif()

if(NOT IS_DIRECTORY ${INSTALL_ROOT})
    message(FATAL_ERROR "No install dir set. Use -DINSTALL_ROOT")
endif()

set(TOP_INST_DIR ${INSTALL_ROOT})
set(EXTPREFIX "${TOP_INST_DIR}")
set(CMAKE_PREFIX_PATH "${EXTPREFIX}")

message( STATUS "CMAKE_GENERATOR: ${CMAKE_GENERATOR}")
message( STATUS "CMAKE_CL_64: ${CMAKE_CL_64}")

set(GLOBAL_BUILD_TYPE RelWithDebInfo)
set(GLOBAL_PROFILE ${GLOBAL_PROFILE} -DBUILD_TESTING=false)
set(PATCH_COMMAND patch)

# this list must be dependency-ordered
add_subdirectory(ext_jpeg)
add_subdirectory(ext_jasper)
add_subdirectory(ext_png)
add_subdirectory(ext_tiff)
#add_subdirectory(ext_libgphoto2)
#add_subdirectory(ext_sane)
add_subdirectory(ext_exiv2)
add_subdirectory(ext_boost)
#add_subdirectory(ext_opencv)
#add_subdirectory(ext_lensfun)
add_subdirectory(ext_qt)
#add_subdirectory(ext_qtwebkit)
#add_subdirectory(ext_qtav)
#add_subdirectory(ext_kf5)
#add_subdirectory(ext_marble)
#add_subdirectory(ext_libksane)
