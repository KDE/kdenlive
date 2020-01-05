project(Kdenlive)

# An odd patch version number means development version, while an even one means
# stable release. An additional number can be used for bugfix-only releases.

# KDE Application Version, managed by release script
set(KDE_APPLICATIONS_VERSION_MAJOR "19")
set(KDE_APPLICATIONS_VERSION_MINOR "12")
set(KDE_APPLICATIONS_VERSION_MICRO "1")

set(KDENLIVE_VERSION ${KDE_APPLICATIONS_VERSION_MAJOR}.${KDE_APPLICATIONS_VERSION_MINOR}.${KDE_APPLICATIONS_VERSION_MICRO})

cmake_minimum_required(VERSION 3.0)
if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()
if(POLICY CMP0053)
    cmake_policy(SET CMP0053 NEW)
endif()

if(BUILD_FUZZING)
  set(CMAKE_CXX_FLAGS "${KDENLIVE_CXX_FLAGS} -fsanitize=fuzzer-no-link,address")
endif()
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option")
endif()

# To be switched on when releasing.
option(RELEASE_BUILD "Remove Git revision from program version" ON)
option(BUILD_TESTING "Build tests" ON)
option(BUILD_FUZZING "Build fuzzing target" OFF)

# Minimum versions of main dependencies.
set(MLT_MIN_MAJOR_VERSION 6)
set(MLT_MIN_MINOR_VERSION 18)
set(MLT_MIN_PATCH_VERSION 0)
set(MLT_MIN_VERSION ${MLT_MIN_MAJOR_VERSION}.${MLT_MIN_MINOR_VERSION}.${MLT_MIN_PATCH_VERSION})

# KDE Frameworks
find_package(ECM 5.45.0 REQUIRED CONFIG)
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
include(ECMEnableSanitizers)
add_definitions(-DTRANSLATION_DOMAIN=\"kdenlive\")
find_package(KF5 REQUIRED COMPONENTS Archive Bookmarks CoreAddons Config ConfigWidgets
                            DBusAddons KIO WidgetsAddons NotifyConfig NewStuff XmlGui Notifications GuiAddons TextWidgets IconThemes Declarative Solid
                 OPTIONAL_COMPONENTS DocTools FileMetaData Crash Purpose)

# Qt
set(QT_MIN_VERSION 5.11.0)
find_package(Qt5 REQUIRED COMPONENTS Core DBus Widgets Svg Quick Concurrent QuickWidgets Multimedia)
add_definitions(-DQT_NO_CAST_TO_ASCII -DQT_NO_URL_CAST_FROM_STRING)
set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")

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
    find_package(Qt5 OPTIONAL_COMPONENTS WebKitWidgets QUIET)
endif()

if(KF5FileMetaData_FOUND)
  message(STATUS "Found KF5 FileMetadata to extract file metadata")
  set(KF5_FILEMETADATA TRUE)
else()
  message(STATUS "KF5 FileMetadata not found, file metadata will not be available")
endif()

if(KF5Purpose_FOUND)
  message(STATUS "Found KF5 Purpose, filesharing enabled")
  set(KF5_PURPOSE TRUE)
else()
  message(STATUS "KF5 Purpose not found, filesharing disabled")
endif()

if(KF5DocTools_FOUND)
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

find_package(RTTR 0.9.6 QUIET)
if(NOT RTTR_FOUND)
  message(STATUS "RTTR not found on system, will download source and build it")
  include(rttr.CMakeLists.txt)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(FFMPEG_SUFFIX "" CACHE STRING "FFmpeg custom suffix")
configure_file(config-kdenlive.h.cmake config-kdenlive.h @ONLY)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wno-suggest-override")

# Sources
add_subdirectory(src)
add_subdirectory(renderer)
add_subdirectory(thumbnailer)
add_subdirectory(data)
ki18n_install(po)

include(GNUInstallDirs)
install(FILES AUTHORS COPYING README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

if (ECM_VERSION VERSION_LESS "5.59.0")
    install(FILES kdenlive.categories DESTINATION ${KDE_INSTALL_CONFDIR})
else()
    install(FILES kdenlive.categories DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR})
endif()

############################
# Tests
############################

if(BUILD_TESTING)
    message(STATUS "Building tests")
    add_subdirectory(tests)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fexceptions")
    include_directories(
        ${CMAKE_BINARY_DIR}
        ${CMAKE_BINARY_DIR}/src
        ${MLT_INCLUDE_DIR}
        ${MLTPP_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/external
        ${CMAKE_CURRENT_SOURCE_DIR}/lib
        src)
    add_executable(runTests ${Tests_SRCS})
    set_property(TARGET runTests PROPERTY CXX_STANDARD 14)
    target_link_libraries(runTests kdenliveLib)
    add_test(NAME runTests COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/runTests -d yes)
endif()

if(BUILD_FUZZING)
    message(STATUS "Building fuzzing")
	set(CMAKE_CXX_COMPILER /usr/bin/clang++)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDENLIVE_CXX_FLAGS} -fsanitize=fuzzer-no-link,address")
    add_subdirectory(fuzzer)
endif()

