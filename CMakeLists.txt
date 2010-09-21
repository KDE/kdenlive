project(kdenlive)

set(BASE_VERSION 0.7.9)
set(LIBMLT_REQUIRED_VERSION 0.5.10)

option(RELEASE_BUILD "Remove compilation date from program version (use for stable releases)" OFF)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# Search packages used by KDE.
find_package(KDE4 REQUIRED)
include(KDE4Defaults)
include(MacroLibrary)
include(FindGettext)
include(MacroOptionalAddSubdirectory)
set(catalogname kdenlive)
add_definitions(${QT_DEFINITIONS})

# Search MLT package.
find_package(LIBMLT REQUIRED)
if(LIBMLT_VERSION VERSION_LESS ${LIBMLT_REQUIRED_VERSION})
  message(FATAL_ERROR "MLT must be at least version ${LIBMLT_REQUIRED_VERSION}, you have ${LIBMLT_VERSION}\nPlease install recent MLT http://www.mltframework.org")
endif(LIBMLT_VERSION VERSION_LESS ${LIBMLT_REQUIRED_VERSION})

find_package(MSGFMT REQUIRED)
find_program(EXTRACTRC_EXECUTABLE extractrc)
find_program(MSGFMT_EXECUTABLE msgfmt)
find_program(MSGMERGE_EXECUTABLE msgmerge)
find_program(XGETTEXT_EXECUTABLE xgettext)

# Get current version.
if(RELEASE_BUILD)
  set(VERSION "\"${BASE_VERSION}\"")
else(RELEASE_BUILD)
  if(EXISTS ${PROJECT_SOURCE_DIR}/.svn)
    # Probably a SVN workspace: determine the revision.
    find_package(Subversion)
    if(Subversion_FOUND)
      Subversion_WC_INFO(${PROJECT_SOURCE_DIR} KDENLIVE)
      message("Current revision is ${KDENLIVE_WC_REVISION}")
      set(VERSION "\"${BASE_VERSION} (rev. ${KDENLIVE_WC_REVISION})\"")
    else(Subversion_FOUND)
      message("Could not determine the SVN revision")
      set(VERSION "\"${BASE_VERSION}\"")
    endif(Subversion_FOUND)
  else(EXISTS ${PROJECT_SOURCE_DIR}/.svn)
    set(VERSION "\"${BASE_VERSION}\"")
  endif(EXISTS ${PROJECT_SOURCE_DIR}/.svn)
endif(RELEASE_BUILD)

add_subdirectory(src src/cmake_bindir)
add_subdirectory(renderer renderer/cmake_bindir)
add_subdirectory(thumbnailer thumbnailer/cmake_bindir)
add_subdirectory(plugins)
add_subdirectory(effects)
add_subdirectory(export)
add_subdirectory(icons)
add_subdirectory(data)
add_subdirectory(lumas)
add_subdirectory(titles)
add_subdirectory(po)
add_subdirectory(man)

configure_file(${CMAKE_SOURCE_DIR}/kdenlive-config.h.cmake kdenlive-config.h @ONLY)
