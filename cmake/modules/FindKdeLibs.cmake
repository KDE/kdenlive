include(FindQt3)
include(FindKDE3)


set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)" FORCE)

SET(LIB_SEARCH_PATHES   ${KDE3_LIB_DIR}
  $ENV{KDEDIR}/lib
  /opt/kde/lib
  /opt/kde3/lib
  /usr/lib
  /usr/local/lib
)

IF (LIB_SUFFIX)
    SET(LIB_SEARCH_PATHES
        $ENV{KDEDIR}/lib${LIB_SUFFIX}
        /opt/kde/lib${LIB_SUFFIX}
        /opt/kde3/lib${LIB_SUFFIX}
        /usr/lib${LIB_SUFFIX}
        /usr/local/lib${LIB_SUFFIX}
        ${LIB_SEARCH_PATHES})
ENDIF (LIB_SUFFIX)

FIND_LIBRARY(KDE3_UI_LIBRARY NAMES kdeui
  PATHS
  ${LIB_SEARCH_PATHES}
)

FIND_LIBRARY(KDE3_PART_LIBRARY NAMES kparts
  PATHS
  ${LIB_SEARCH_PATHES}
)

FIND_LIBRARY(KDE3_KIO_LIBRARY NAMES kio
  PATHS
  ${LIB_SEARCH_PATHES}
)

FIND_LIBRARY(KDE3_DCOP_LIBRARY NAMES DCOP
  PATHS
  ${LIB_SEARCH_PATHES}
)

FIND_LIBRARY(KDE3_WALLET_LIBRARY NAMES kwalletclient
  PATHS
  ${LIB_SEARCH_PATHES}
)

FIND_LIBRARY(KDE3_KINIT_KDED_LIBRARY NAMES kdeinit_kded
  PATHS
  ${LIB_SEARCH_PATHES}
)

FIND_PROGRAM(KDE3_KDECONFIG_EXECUTABLE NAME kde-config PATHS
    $ENV{KDEDIR}/bin
    /opt/kde/bin
    /opt/kde3/bin
)

FIND_PROGRAM(MSGFMT
    NAMES gmsgfmt msgfmt)

FIND_PROGRAM(STRIP
    NAMES strip)

FIND_PROGRAM(KDE3_MEINPROC_EXECUTABLE NAME meinproc PATHS
     ${KDE3_BIN_INSTALL_DIR}
     $ENV{KDEDIR}/bin
     /opt/kde/bin
     /opt/kde3/bin
)

IF(KDE3_MEINPROC_EXECUTABLE)
    MESSAGE(STATUS "Found meinproc: ${KDE3_MEINPROC_EXECUTABLE}")
ELSE(KDE3_MEINPROC_EXECUTABLE)
    MESSAGE(STATUS "Didn't find meinproc!")
ENDIF(KDE3_MEINPROC_EXECUTABLE)

IF(MSGFMT)
    EXECUTE_PROCESS(COMMAND ${MSGFMT} "--version" "2>&1"
    OUTPUT_VARIABLE _msgout)
    STRING(REGEX MATCH "GNU[\t ]gettext" _isgnu "${_msgout}")
    IF (NOT _isgnu)
        MESSAGE(STATUS "No gnu msgfmt found!")
        SET(MSGFMT ":" CACHE STRING "Msgfmt program")
    ELSE(NOT _isgnu)
        MESSAGE(STATUS "Found gnu msgfmt: ${MSGFMT}")
    ENDIF (NOT _isgnu)
ELSE(MSGFMT)
    SET(MSGFMT ":" CACHE STRING "Msgfmt program")
ENDIF(MSGFMT)

# 'cause my own defines were not good I take them from kde4 trunk
#add some KDE specific stuff
set(SHARE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX}/share CACHE PATH "Base directory for files which go to share/" FORCE)
set(EXEC_INSTALL_PREFIX  ${CMAKE_INSTALL_PREFIX}       CACHE PATH  "Base directory for executables and libraries" FORCE)
#
## the following are directories where stuff will be installed to
set(BIN_INSTALL_DIR          "${EXEC_INSTALL_PREFIX}/bin"                  CACHE PATH "The kde bin install dir (default prefix/bin)" FORCE)
set(SBIN_INSTALL_DIR         "${EXEC_INSTALL_PREFIX}/sbin"                 CACHE PATH "The kde sbin install dir (default prefix/sbin)" FORCE)
set(LIB_INSTALL_DIR          "${EXEC_INSTALL_PREFIX}/lib${LIB_SUFFIX}"     CACHE PATH "The subdirectory relative to the install prefix where libraries will be installed (default is /lib${LIB_SUFFIX})" FORCE)
set(LIBEXEC_INSTALL_DIR      "${LIB_INSTALL_DIR}/kde3/libexec"             CACHE PATH "The subdirectory relative to the install prefix where libraries will be installed (default is /lib/kde3/libexec)" FORCE)
set(PLUGIN_INSTALL_DIR       "${LIB_INSTALL_DIR}/kde3"                     CACHE PATH "The subdirectory relative to the install prefix where plugins will be installed (default is ${LIB_INSTALL_DIR}/kde3)" FORCE)
set(INCLUDE_INSTALL_DIR      "${CMAKE_INSTALL_PREFIX}/include"             CACHE PATH "The subdirectory to the header prefix" FORCE)
set(CONFIG_INSTALL_DIR       "${SHARE_INSTALL_PREFIX}/config"              CACHE PATH "The config file install dir" FORCE)
set(DATA_INSTALL_DIR         "${SHARE_INSTALL_PREFIX}/apps"                CACHE PATH "The parent directory where applications can install their data" FORCE)
set(HTML_INSTALL_DIR         "${SHARE_INSTALL_PREFIX}/doc/HTML"            CACHE PATH "The HTML install dir for documentation"  FORCE)
set(ICON_INSTALL_DIR         "${SHARE_INSTALL_PREFIX}/icons"               CACHE PATH "The icon install dir (default prefix/share/icons/)" FORCE)
set(KCFG_INSTALL_DIR         "${SHARE_INSTALL_PREFIX}/config.kcfg"         CACHE PATH "The install dir for kconfig files" FORCE)
set(LOCALE_INSTALL_DIR       "${SHARE_INSTALL_PREFIX}/locale"              CACHE PATH "The install dir for translations" FORCE)
set(MIME_INSTALL_DIR         "${SHARE_INSTALL_PREFIX}/mimelnk"             CACHE PATH "The install dir for the mimetype desktop files" FORCE)
set(SERVICES_INSTALL_DIR     "${SHARE_INSTALL_PREFIX}/services"            CACHE PATH "The install dir for service (desktop, protocol, ...) files" FORCE)
set(SERVICETYPES_INSTALL_DIR "${SHARE_INSTALL_PREFIX}/servicetypes"        CACHE PATH "The install dir for servicestypes desktop files" FORCE)
set(SOUND_INSTALL_DIR        "${SHARE_INSTALL_PREFIX}/sounds"              CACHE PATH "The install dir for sound files" FORCE)
set(TEMPLATES_INSTALL_DIR    "${SHARE_INSTALL_PREFIX}/templates"           CACHE PATH "The install dir for templates (Create new file...)" FORCE)
set(WALLPAPER_INSTALL_DIR    "${SHARE_INSTALL_PREFIX}/wallpapers"          CACHE PATH "The install dir for wallpapers" FORCE)
set(KCONF_UPDATE_INSTALL_DIR "${DATA_INSTALL_DIR}/kconf_update"            CACHE PATH "The kconf_update install dir" FORCE)
# this one shouldn't be used anymore
set(APPLNK_INSTALL_DIR       "${SHARE_INSTALL_PREFIX}/applnk"              CACHE PATH "Is this still used ?" FORCE)
set(AUTOSTART_INSTALL_DIR    "${SHARE_INSTALL_PREFIX}/autostart"           CACHE PATH "The install dir for autostart files" FORCE)
set(XDG_APPS_DIR             "${SHARE_INSTALL_PREFIX}/applications/kde"    CACHE PATH "The XDG apps dir" FORCE)
set(XDG_DIRECTORY_DIR        "${SHARE_INSTALL_PREFIX}/desktop-directories" CACHE PATH "The XDG directory" FORCE)
set(SYSCONF_INSTALL_DIR      "${CMAKE_INSTALL_PREFIX}/etc"                 CACHE PATH "The kde sysconfig install dir (default/etc)" FORCE)
set(MAN_INSTALL_DIR          "${SHARE_INSTALL_PREFIX}/man"                 CACHE PATH "The kde man install dir (default prefix/share/man/)" FORCE)
set(INFO_INSTALL_DIR         "${CMAKE_INSTALL_PREFIX}/info"                CACHE PATH "The kde info install dir (default prefix/info)" FORCE)


MACRO(ADD_POFILES packagename)
    SET(_gmofiles)
    FILE(GLOB _pofiles *.po)

    FOREACH(_current_po ${_pofiles})
        GET_FILENAME_COMPONENT(_name ${_current_po} NAME_WE)
        STRING(REGEX REPLACE "^.*/([a-zA-Z]+)(\\.po)" "\\1" _lang "${_current_po}")
        SET(_gmofile "${CMAKE_CURRENT_BINARY_DIR}/${_name}.gmo")
        ADD_CUSTOM_COMMAND(OUTPUT ${_gmofile}
            COMMAND ${MSGFMT}
            ARGS "-o" "${_gmofile}" "${_current_po}"
            DEPENDS ${_current_po}
            )
        SET(_gmofiles ${_gmofiles} ${_gmofile})
        INSTALL(FILES ${_gmofile}
            DESTINATION ${LOCALE_INSTALL_DIR}/${_lang}/LC_MESSAGES
            RENAME ${packagename}.mo)
    ENDFOREACH(_current_po ${_pofiles})

    ADD_CUSTOM_TARGET(translations ALL
        DEPENDS ${_gmofiles})

ENDMACRO(ADD_POFILES)
