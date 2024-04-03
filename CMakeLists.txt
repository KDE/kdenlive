# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Julius Künzel <jk.kdedev@smartlab.uber.space>, Jean-Baptiste Mardelle <jb@kdenlive.org>, Alberto Villa <avilla@FreeBSD.org>, Albert Astals Cid <aacid@kde.org>, Vincent Pinon <vpinon@kde.org>, Laurent Montel <montel@kde.org>, Vincent Pinon <vincent.pinon@asygn.com>, Nicolas Carion <french.ebook.lover@gmail.com>

cmake_minimum_required(VERSION 3.16)

# An odd patch version number means development version, while an even one means
# stable release. An additional number can be used for bugfix-only releases.

# KDE Application Version, managed by release script
set (RELEASE_SERVICE_VERSION_MAJOR "24")
set (RELEASE_SERVICE_VERSION_MINOR "04")
set (RELEASE_SERVICE_VERSION_MICRO "70")

set(KDENLIVE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

project(Kdenlive VERSION ${KDENLIVE_VERSION})

include(FeatureSummary)
include(GenerateExportHeader)

# Register CMake options
option(RELEASE_BUILD "Remove Git revision from program version" ON) # To be switched on when releasing.
option(BUILD_TESTING "Build tests" ON)
option(CRASH_AUTO_TEST "Auto-generate testcases upon some crashes (uses RTTR library, needed for fuzzing)" OFF)
option(BUILD_FUZZING "Build fuzzing target" OFF)
option(NODBUS "Build without DBus IPC" OFF)
option(USE_VERSIONLESS_TARGETS "Use versionless targets" OFF)
option(BUILD_QCH "Build source code documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)" OFF)
add_feature_info(QCH ${BUILD_QCH} "Source code documentation in QCH format (for e.g. Qt Assistant, Qt Creator & KDevelop)")

option(BUILD_DESIGNERPLUGIN "Build plugin for Qt Designer" OFF)

# Minimum versions of main dependencies.
set(MLT_MIN_MAJOR_VERSION 7)
set(MLT_MIN_MINOR_VERSION 22)
set(MLT_MIN_PATCH_VERSION 0)
set(MLT_MIN_VERSION ${MLT_MIN_MAJOR_VERSION}.${MLT_MIN_MINOR_VERSION}.${MLT_MIN_PATCH_VERSION})

# Qt version
set(QT_MIN_VERSION 5.15.2)
set(KF_DEP_VERSION "5.92.0")

find_package(ECM ${KF_DEP_VERSION} REQUIRED CONFIG)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/modules)

# Select the Qt version we want to use
# Qt6 is the prefered one, Qt5 the fallback
if (NOT DEFINED QT_MAJOR_VERSION)
    if (NOT WIN32)
      find_package(Qt6 COMPONENTS Core QUIET)
      if (TARGET Qt6::Core)
          set(QT_MAJOR_VERSION 6)
      else()
          find_package(Qt5 COMPONENTS Core QUIET)
          if (TARGET Qt5::Core)
            set(QT_MAJOR_VERSION 5)
          endif()
      endif()
    endif()
    if(NOT QT_MAJOR_VERSION)
        option(BUILD_WITH_QT6 "Build against Qt 6" ON)
        if (BUILD_WITH_QT6)
            set(QT_MAJOR_VERSION 6)
        else()
            set(QT_MAJOR_VERSION 5)
        endif()
    endif()
endif()

## include ECM Modules
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMInstallIcons)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMOptionalAddSubdirectory)
include(ECMMarkNonGuiExecutable)
include(ECMAddAppIcon)
include(ECMQtDeclareLoggingCategory)
include(ECMEnableSanitizers)
include(ECMAddQch)
include(ECMDeprecationSettings)
remove_definitions(-DQT_NO_CAST_FROM_ASCII) # Defined in KDECompilerSettings, but we don't want that yet
# MLT uses variadic macros upstream and hence the build log gets spamed, so disable the warning
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-variadic-macros")

if (QT_MAJOR_VERSION STREQUAL "6")
    set(USE_VERSIONLESS_TARGETS ON)
endif()

# KDE Frameworks
if (NOT DEFINED KF_MAJOR)
  set(KF_MAJOR ${QT_MAJOR_VERSION})
endif()

if (KF_MAJOR STREQUAL "6")
  set(KF_DEP_VERSION "5.240.0")
endif()

add_definitions(-DTRANSLATION_DOMAIN=\"kdenlive\")
find_package(KF${KF_MAJOR} ${KF_DEP_VERSION}
    REQUIRED COMPONENTS
    Archive
    Bookmarks
    Codecs
    CoreAddons
    Config
    ConfigWidgets
    KIO
    WidgetsAddons
    NotifyConfig
    NewStuff
    XmlGui
    Notifications
    GuiAddons
    TextWidgets
    IconThemes
    Solid
    FileMetaData
    Purpose

    OPTIONAL_COMPONENTS
    DocTools
    Crash
)

if (KF_MAJOR LESS "6")
  find_package(KF${KF_MAJOR} ${KF_DEP_VERSION}
      REQUIRED COMPONENTS
      Declarative
  )
endif()

ecm_set_disabled_deprecation_versions(
    QT ${QT_MIN_VERSION}
    KF ${KF_DEP_VERSION}
)

# Qt targets
find_package(Qt${QT_MAJOR_VERSION}
    REQUIRED COMPONENTS
    Core Widgets
    Svg
    Quick
    QuickControls2
    Concurrent
    QuickWidgets
    Multimedia
    NetworkAuth
)
if (QT_MAJOR_VERSION STREQUAL "6")
    find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} REQUIRED NO_MODULE COMPONENTS SvgWidgets)
endif()
if(NOT NODBUS)
    find_package(KF${KF_MAJOR} ${KF_DEP_VERSION} REQUIRED COMPONENTS DBusAddons)
    find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS DBus)
endif()
add_definitions(-DQT_NO_CAST_TO_ASCII -DQT_NO_URL_CAST_FROM_STRING)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt${QT_MAJOR_VERSION}Widgets_EXECUTABLE_COMPILE_FLAGS}")

# MLT
find_package(MLT ${MLT_MIN_VERSION} REQUIRED)
set_package_properties(MLT PROPERTIES DESCRIPTION "Multimedia framework"
                URL "https://mltframework.org"
                PURPOSE "Required to do video processing")
message(STATUS "Found MLT++: ${MLTPP_LIBRARIES}")

# Windows
include(CheckIncludeFiles)
check_include_files(malloc.h HAVE_MALLOC_H)
check_include_files(pthread.h HAVE_PTHREAD_H)
if(WIN32)
    find_package(DrMinGW)
    set(MLT_PREFIX "..")
else()
    set(MLT_PREFIX ${MLT_ROOT_DIR})
endif()

# macOS
if(APPLE)
    set(DATA_INSTALL_PREFIX "")
else()
    set(DATA_INSTALL_PREFIX "/kdenlive")
endif()

if(KF${KF_MAJOR}DocTools_FOUND)
    add_subdirectory(doc)
    kdoctools_install(po)
endif()

# Get current version.
set(KDENLIVE_VERSION_STRING "${KDENLIVE_VERSION}")
if(NOT RELEASE_BUILD AND EXISTS ${CMAKE_SOURCE_DIR}/.git)
  # Probably a Git workspace; determine the revision.
  find_package(Git QUIET)
  if(GIT_FOUND)
    exec_program(${GIT_EXECUTABLE} ${CMAKE_SOURCE_DIR}
	  ARGS "log -n 1 --pretty=format:\"%h\""
      OUTPUT_VARIABLE KDENLIVE_GIT_REVISION)
    message(STATUS "Kdenlive Git revision: ${KDENLIVE_GIT_REVISION}")
    set(KDENLIVE_VERSION_STRING "${KDENLIVE_VERSION} (rev. ${KDENLIVE_GIT_REVISION})")
  else()
    message(STATUS "Kdenlive Git revision could not be determined")
  endif()
endif()

if(CRASH_AUTO_TEST)
  if(BUILD_DESIGNERPLUGIN)
    message(SEND_ERROR "The options CRASH_AUTO_TEST and BUILD_DESIGNERPLUGIN do not work together, disable one of both.")
  endif()
  find_package(RTTR 0.9.6 QUIET)
  if(NOT RTTR_FOUND)
    message(STATUS "RTTR not found on system, will download source and build it")
    include(rttr.CMakeLists.txt)
  endif()
  if(BUILD_FUZZING)
    set(ECM_ENABLE_SANITIZERS fuzzer;address)
  endif()
endif()

set(FFMPEG_SUFFIX "" CACHE STRING "FFmpeg custom suffix")
configure_file(config-kdenlive.h.cmake config-kdenlive.h @ONLY)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

# Sources
add_subdirectory(src)
add_subdirectory(renderer)
if(KF${KF_MAJOR}GuiAddons_VERSION VERSION_LESS "5.100")
  add_subdirectory(thumbnailer-deprecated)
else()
  add_subdirectory(thumbnailer)
endif()
add_subdirectory(data)

# Install
ki18n_install(po)

if (BUILD_QCH)
    ecm_install_qch_export(
        TARGETS Kdenlive_QCH
        FILE KdenliveQCHTargets.cmake
        DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/kdenlive"
        COMPONENT Devel
    )
endif()

include(GNUInstallDirs)
install(FILES AUTHORS README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(DIRECTORY LICENSES DESTINATION ${CMAKE_INSTALL_DOCDIR})

ecm_qt_install_logging_categories(
    EXPORT KDENLIVE
    FILE kdenlive.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
)

# Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
if(BUILD_FUZZING AND ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
    add_subdirectory(fuzzer)
elseif(BUILD_FUZZING)
    message(STATUS "Fuzzing build was requested but not enabled because compiler is ${CMAKE_CXX_COMPILER_ID} and not Clang")
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

# pre-commit hook
include(KDEGitCommitHooks)
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

