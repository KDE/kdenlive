project(Kdenlive)

# An odd patch version number means development version, while an even one means
# stable release. An additional number can be used for bugfix-only releases.

# KDE Application Version, managed by release script
set (KDE_APPLICATIONS_VERSION_MAJOR "17")
set (KDE_APPLICATIONS_VERSION_MINOR "07")
set (KDE_APPLICATIONS_VERSION_MICRO "70")

set(KDENLIVE_VERSION ${KDE_APPLICATIONS_VERSION_MAJOR}.${KDE_APPLICATIONS_VERSION_MINOR}.${KDE_APPLICATIONS_VERSION_MICRO})
cmake_minimum_required(VERSION 3.0)
if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()
if (POLICY CMP0053)
    cmake_policy(SET CMP0053 NEW)
endif()
# Minimum versions of main dependencies.
set(MLT_MIN_MAJOR_VERSION 6)
set(MLT_MIN_MINOR_VERSION 4)
set(MLT_MIN_PATCH_VERSION 0)
set(MLT_MIN_VERSION ${MLT_MIN_MAJOR_VERSION}.${MLT_MIN_MINOR_VERSION}.${MLT_MIN_PATCH_VERSION})

set(QT_MIN_VERSION 5.6.0)

find_package(ECM 5.18.0 REQUIRED CONFIG)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)
include(ECMInstallIcons)
include(GenerateExportHeader)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMOptionalAddSubdirectory)
include(ECMMarkNonGuiExecutable)
include(ECMAddAppIcon)
include(ECMQtDeclareLoggingCategory)

add_definitions(-DQT_NO_CAST_TO_ASCII)
add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DTRANSLATION_DOMAIN=\"kdenlive\")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
# To be switched on when releasing.
option(RELEASE_BUILD "Remove Git revision from program version (use for stable releases)" ON)

# Get current version.
set(KDENLIVE_VERSION_STRING "${KDENLIVE_VERSION}")
if(NOT RELEASE_BUILD AND EXISTS ${CMAKE_SOURCE_DIR}/.git)
  # Probably a Git workspace; determine the revision.
  find_package(Git QUIET)
  if(GIT_FOUND)
    exec_program(${GIT_EXECUTABLE}
      ${CMAKE_SOURCE_DIR}
	ARGS "log -n 1 --pretty=format:\"%h\""
      OUTPUT_VARIABLE KDENLIVE_GIT_REVISION
      RETURN_VALUE TAG_RESULT
    )

    # git log failed; maybe the repository was checked with depth=1.
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

find_package(Qt5 REQUIRED COMPONENTS Core DBus Widgets Script Svg Quick )
find_package(Qt5 OPTIONAL_COMPONENTS WebKitWidgets QUIET)

find_package(KF5 5.23.0 OPTIONAL_COMPONENTS XmlGui QUIET)
if (KF5XmlGui_FOUND)
  message(STATUS "Found KF5 >= 5.23.0 enabling icon coloring")
else()
  message(STATUS "KF5 < 5.23.0 Disable icon coloring")
  set(KF5_ICON_COMPATIBILITY TRUE)
endif()

find_package(KF5 REQUIRED COMPONENTS Archive Bookmarks CoreAddons Config ConfigWidgets 
                            DBusAddons KIO WidgetsAddons NotifyConfig NewStuff XmlGui Notifications GuiAddons TextWidgets IconThemes Crash
                 OPTIONAL_COMPONENTS DocTools FileMetaData)

if (KF5FileMetaData_FOUND)
  message(STATUS "Found KF5 FileMetadata to extract file metadata")
  set(KF5_FILEMETADATA TRUE)
else()
  message(STATUS "KF5 FileMetadata not found, file metadata will not be available")
endif()

# Search MLT package.
find_package(MLT ${MLT_MIN_VERSION} REQUIRED)
set_package_properties(MLT PROPERTIES 
                DESCRIPTION "Multimedia framework and video playout server for TV broadcasting"
                URL "http://mltframework.org"
                TYPE RUNTIME
                PURPOSE "Required to do video processing")

set(MLT_PREFIX ${MLT_ROOT_DIR})

add_subdirectory(data)
if(KF5DocTools_FOUND)
    add_subdirectory(doc)
endif()
#add_subdirectory(plugins)
ecm_optional_add_subdirectory(po)
add_subdirectory(renderer)
add_subdirectory(src)
add_subdirectory(thumbnailer)
#add_subdirectory(testingArea)


install( FILES kdenlive.categories DESTINATION ${KDE_INSTALL_CONFDIR} )

configure_file(config-kdenlive.h.cmake config-kdenlive.h @ONLY)
feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
