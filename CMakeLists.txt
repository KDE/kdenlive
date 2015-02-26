project(Kdenlive)

# An odd patch version number means development version, while an even one means
# stable release. An additional number can be used for bugfix-only releases.
set(KDENLIVE_VERSION 15.03.80)
cmake_minimum_required(VERSION 2.8.12)
# Minimum versions of main dependencies.
set(MLT_MIN_MAJOR_VERSION 0)
set(MLT_MIN_MINOR_VERSION 9)
set(MLT_MIN_PATCH_VERSION 4)
set(MLT_MIN_VERSION ${MLT_MIN_MAJOR_VERSION}.${MLT_MIN_MINOR_VERSION}.${MLT_MIN_PATCH_VERSION})

set(QT_MIN_VERSION 5.2.0)

find_package(ECM 1.2.0 REQUIRED CONFIG)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)
include(FeatureSummary)
include(ECMInstallIcons)
include(GenerateExportHeader)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
SET(CMAKE_CXX_FLAGS "-fPIC")
# To be switched on when releasing.
option(RELEASE_BUILD "Remove Git revision from program version (use for stable releases)" OFF)

# Get current version.
set(KDENLIVE_VERSION_STRING "${KDENLIVE_VERSION}")
if(NOT RELEASE_BUILD AND EXISTS ${CMAKE_SOURCE_DIR}/.git)
  # Probably a Git workspace; determine the revision.
  find_package(Git QUIET)
  if(GIT_FOUND)
    exec_program(${GIT_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}
      ARGS "describe --tags"
      OUTPUT_VARIABLE KDENLIVE_GIT_REVISION
      RETURN_VALUE TAG_RESULT
    )
    # git describe --tags failed; maybe the repository was checked with depth=1.
    if(NOT ${TAG_RESULT} EQUAL 0)
      exec_program(${GIT_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}
        ARGS "describe --always"
        OUTPUT_VARIABLE KDENLIVE_GIT_REVISION
      )
    endif()
    message(STATUS "Kdenlive Git revision: ${KDENLIVE_GIT_REVISION}")
    set(KDENLIVE_VERSION_STRING "${KDENLIVE_VERSION} (rev. ${KDENLIVE_GIT_REVISION})")
  else()
    message(STATUS "Kdenlive Git revision could not be determined")
  endif()
endif()

include(CheckIncludeFiles)
check_include_files(malloc.h HAVE_MALLOC_H)
check_include_files(pthread.h HAVE_PTHREAD_H)

find_package(Qt5 CONFIG REQUIRED Core DBus Test Widgets Script Svg OpenGL)

find_package(KF5 REQUIRED Archive Bookmarks CoreAddons Config ConfigWidgets 
                            DBusAddons KIO WidgetsAddons Plotting NotifyConfig NewStuff XmlGui Notifications GuiAddons TextWidgets IconThemes DocTools)


# Search MLT package.
find_package(MLT ${MLT_MIN_VERSION} REQUIRED)
set_package_properties(MLT PROPERTIES 
                DESCRIPTION "Multimedia framework and video playout server for TV broadcasting"
                URL "http://mltframework.org"
                TYPE RUNTIME
                PURPOSE "Required to do video processing")

set(MLT_PREFIX ${MLT_ROOT_DIR})

add_subdirectory(data)
add_subdirectory(doc)
#add_subdirectory(plugins)
#ecm_optional_add_subdirectory(po)
add_subdirectory(renderer)
add_subdirectory(src)
add_subdirectory(thumbnailer)
#add_subdirectory(testingArea)



configure_file(config-kdenlive.h.cmake config-kdenlive.h @ONLY)
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
