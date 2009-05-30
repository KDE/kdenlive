project(kdenlive)

SET(BASE_VERSION 0.7.4)

OPTION(RELEASE_BUILD "Remove compilation date to version, use for stable releases (default off)" ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

# search packages used by KDE
find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)
INCLUDE(FindGettext)
include(MacroOptionalAddSubdirectory)
set(catalogname kdenlive)

add_definitions (${QT_DEFINITIONS})

FIND_PACKAGE(LIBMLT REQUIRED)
find_package(MSGFMT REQUIRED)

find_program(EXTRACTRC_EXECUTABLE extractrc)
find_program(MSGFMT_EXECUTABLE msgfmt)
find_program(MSGMERGE_EXECUTABLE msgmerge)
find_program(XGETTEXT_EXECUTABLE xgettext)

# current version
IF (RELEASE_BUILD)
    SET(VERSION "\"${BASE_VERSION}\"")
ELSE(RELEASE_BUILD)
   IF (EXISTS ${PROJECT_SOURCE_DIR}/.svn) 
      # Probably a SVN workspace, determine revision level 
      FIND_PACKAGE(Subversion) 
      IF(Subversion_FOUND) 
        Subversion_WC_INFO(${PROJECT_SOURCE_DIR} KDENLIVE) 
        MESSAGE("Current revision is ${KDENLIVE_WC_REVISION}") 
	SET(VERSION "\"${BASE_VERSION} (rev. ${KDENLIVE_WC_REVISION})\"")
      ELSE(Subversion_FOUND)
	MESSAGE("Could not determine SVN revision")
	SET(VERSION "\"${BASE_VERSION}\"")
      ENDIF(Subversion_FOUND) 
    ELSE(EXISTS ${PROJECT_SOURCE_DIR}/.svn)
    SET(VERSION "\"${BASE_VERSION}\"")
    ENDIF(EXISTS ${PROJECT_SOURCE_DIR}/.svn)
ENDIF(RELEASE_BUILD)

add_subdirectory(src src/cmake_bindir)
add_subdirectory(renderer renderer/cmake_bindir)
add_subdirectory(thumbnailer thumbnailer/cmake_bindir)
add_subdirectory(plugins)
add_subdirectory(effects)
add_subdirectory(export) 
add_subdirectory(icons)
add_subdirectory(data)  
add_subdirectory(lumas) 
add_subdirectory(po)
add_subdirectory(man)

CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/kdenlive-config.h.cmake kdenlive-config.h @ONLY)

