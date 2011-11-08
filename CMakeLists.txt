project(kdenlive)
set(catalogname kdenlive)

# An odd minor version number means development version, while an even
# one means stable release.
set(KDENLIVE_VERSION 0.8.3)
set(LIBMLT_REQUIRED_VERSION 0.7.6)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# To be switched on when releasing.
option(RELEASE_BUILD "Remove Git position from program version (use for stable releases)" OFF)

# Get current version.
set(VERSION "\"${KDENLIVE_VERSION}\"")
if(NOT RELEASE_BUILD)
  if(EXISTS ${PROJECT_SOURCE_DIR}/.git)
    # Probably a Git workspace: determine the revision.
    find_package(Git)
    if(GIT_FOUND)
      exec_program(${GIT_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ARGS "describe --tags"
        OUTPUT_VARIABLE KDENLIVE_GIT_REVISION
      )
      message("Current Kdenlive Git revision is ${KDENLIVE_GIT_REVISION}")
      set(VERSION "\"${KDENLIVE_VERSION} (rev. ${KDENLIVE_GIT_REVISION})\"")
    else(GIT_FOUND)
      message("Could not determine the Git revision")
    endif(GIT_FOUND)
  endif(EXISTS ${PROJECT_SOURCE_DIR}/.git)
endif(NOT RELEASE_BUILD)

include(CheckIncludeFiles)
check_include_files(malloc.h HAVE_MALLOC_H)
check_include_files(pthread.h HAVE_PTHREAD_H)

find_package(MSGFMT REQUIRED)
find_program(EXTRACTRC_EXECUTABLE extractrc)
find_program(MSGFMT_EXECUTABLE msgfmt)
find_program(MSGMERGE_EXECUTABLE msgmerge)
find_program(XGETTEXT_EXECUTABLE xgettext)

# Search KDE and dependent packages.
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
include(FindGettext)

# Search MLT package.
find_package(LIBMLT REQUIRED)
if(LIBMLT_VERSION VERSION_LESS ${LIBMLT_REQUIRED_VERSION})
  set(LIBMLT_FOUND FALSE)
endif(LIBMLT_VERSION VERSION_LESS ${LIBMLT_REQUIRED_VERSION})
macro_log_feature(LIBMLT_FOUND "MLT" "Multimedia framework and video playout server for TV broadcasting" "http://mltframework.org" TRUE ${LIBMLT_REQUIRED_VERSION})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99") # To compile kiss_fft
find_package(Qt4 COMPONENTS QtCore QtGui QtOpenGL QtScript REQUIRED)
if ( NOT QT_QTOPENGL_FOUND )
  message(FATAL_ERROR "QtOpenGL (most likely called libqt4-opengl-dev) not found.")
else ( NOT QT_QTOPENGL_FOUND )
  message(-- " Found QtOpenGL.")
endif ( NOT QT_QTOPENGL_FOUND )
find_library(Qt COMPONENTS QtOpenGL QtScript REQUIRED)

add_definitions(${QT_DEFINITIONS})

add_subdirectory(data)
add_subdirectory(doc)
add_subdirectory(effects)
add_subdirectory(export)
add_subdirectory(icons)
add_subdirectory(lumas)
add_subdirectory(man)
add_subdirectory(plugins)
add_subdirectory(po)
add_subdirectory(renderer renderer/cmake_bindir)
add_subdirectory(src src/cmake_bindir)
add_subdirectory(thumbnailer thumbnailer/cmake_bindir)
add_subdirectory(titles)

macro_display_feature_log()

configure_file(kdenlive-config.h.cmake kdenlive-config.h @ONLY)
