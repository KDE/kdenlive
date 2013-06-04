project(kdenlive)

# An odd patch version number means development version, while an even one means
# stable release. An additional number can be used for bugfix-only releases.
set(KDENLIVE_VERSION 0.8.3)

# Minimum versions of main dependencies.
set(LIBMLT_MIN_VERSION 0.7.6)
set(QT_MIN_VERSION 4.5.0)
set(KDE_MIN_VERSION 4.3.0)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# To be switched on when releasing.
option(RELEASE_BUILD "Remove Git revision from program version (use for stable releases)" OFF)

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
        RETURN_VALUE TAG_RESULT
      )
      # git describe --tags failed, for example it fails if repo was checked with depth=1
      if(NOT ${TAG_RESULT} EQUAL 0)
        exec_program(${GIT_EXECUTABLE}
          ${CMAKE_CURRENT_SOURCE_DIR}
          ARGS "describe --always"
          OUTPUT_VARIABLE KDENLIVE_GIT_REVISION
        )
      endif(NOT ${TAG_RESULT} EQUAL 0)
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

# Search KDE and dependent packages.
find_package(KDE4 ${KDE_MIN_VERSION} REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
macro_log_feature(KDE4_FOUND
  "KDELibs"
  "KDE core libraries"
  "http://www.kde.org"
  TRUE
  ${KDE_MIN_VERSION}
)

# Find Qt core modules (QtCore and QtGui).
find_package(Qt4 ${QT_MIN_VERSION} REQUIRED)

# Search MLT package.
find_package(LIBMLT ${LIBMLT_MIN_VERSION} REQUIRED)
if(LIBMLT_VERSION VERSION_LESS ${LIBMLT_MIN_VERSION})
  set(LIBMLT_FOUND FALSE)
endif(LIBMLT_VERSION VERSION_LESS ${LIBMLT_MIN_VERSION})
macro_log_feature(LIBMLT_FOUND
  "MLT"
  "Multimedia framework and video playout server for TV broadcasting"
  "http://mltframework.org"
  TRUE
  ${LIBMLT_MIN_VERSION}
)

add_subdirectory(data)
macro_optional_add_subdirectory(doc)
add_subdirectory(effects)
add_subdirectory(export)
add_subdirectory(icons)
add_subdirectory(lumas)
add_subdirectory(man)
add_subdirectory(plugins)
macro_optional_add_subdirectory(po)
add_subdirectory(renderer)
add_subdirectory(src)
add_subdirectory(thumbnailer)
add_subdirectory(titles)
# add_subdirectory(testingArea)

macro_display_feature_log()

configure_file(config-kdenlive.h.cmake config-kdenlive.h @ONLY)

add_subdirectory(producers)