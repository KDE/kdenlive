# Script to build KF5 Frameworks for digiKam bundle.
#
# Copyright (c) 2015-2020 by Gilles Caulier  <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

set(EXTPREFIX_frameworks "${EXTPREFIX}")

function(InstallKF5Component
         name
         version
         argoptions
         patch)

    if(MINGW)

        # Extract MD5 sums of each KF5 components directly from server.
        execute_process(COMMAND
                        wget -q -O - -o /dev/null http://download.kde.org/stable/frameworks/${version}/${name}-${version}.0.tar.xz.mirrorlist
                        COMMAND
                        grep "MD5 Hash"
                        COMMAND
                        grep -o -P "(?<=<tt>).*(?=</tt>)"
                        RESULT_VARIABLE
                        wgetresult
                        OUTPUT_VARIABLE
                        md5sm
                        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(NOT ${wgetresult} MATCHES 0)
            message(WARNING "Cannot download MD5 sum for ${name}!")
        endif()

        message(STATUS "--------------------------------")
        message(STATUS "KF5 component    : ${name}")
        message(STATUS "Component version: ${version}")
        message(STATUS "Component MD5    : ${md5sm}")
        message(STATUS "Configure options: ${argoptions}")
        message(STATUS "Patch to apply   : ${patch}")

        if(NOT "${patch}" STREQUAL "")
            set(patch "${PATCH_COMMAND};-p1;-i;${CMAKE_CURRENT_SOURCE_DIR}/${patch}")
        endif()

        ExternalProject_Add(ext_${name}
                            DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
                            URL file://${EXTERNALS_DOWNLOAD_DIR}/${name}-${version}.0.tar.xz
                            URL_MD5 ${md5sm}

                            INSTALL_DIR ${EXTPREFIX_frameworks}

                            CMAKE_ARGS -DMXE_TOOLCHAIN=${MXE_TOOLCHAIN}
                                       -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE}
                                       -DCMAKE_COLOR_MAKEFILE=ON
                                       -DCMAKE_INSTALL_PREFIX=${EXTPREFIX_frameworks}
                                       -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON
                                       -DCMAKE_TOOLCHAIN_FILE=${MXE_TOOLCHAIN}
                                       -DCMAKE_FIND_PREFIX_PATH=${CMAKE_PREFIX_PATH}
                                       -DCMAKE_SYSTEM_INCLUDE_PATH=${CMAKE_PREFIX_PATH}/include
                                       -DCMAKE_INCLUDE_PATH=${CMAKE_PREFIX_PATH}/include
                                       -DCMAKE_LIBRARY_PATH=${CMAKE_PREFIX_PATH}/lib
                                       -DBUILD_TESTING=OFF
                                       -DZLIB_ROOT=${CMAKE_PREFIX_PATH}
                                       -DINSTALL_ROOT=${MXE_INSTALL_PREFIX}
                                       ${GLOBAL_PROFILE}
                                       "${argoptions}"
                                       -Wno-dev

                            PATCH_COMMAND ${patch}
                            UPDATE_COMMAND ""
                            ALWAYS 0
        )

        ExternalProject_Add_step(ext_${name} before_download

            # Note : under MXE, curl is not compiled with https support. We cannot use the tarball as under Linux/MacOS.
            COMMAND wget -N http://download.kde.org/stable/frameworks/${version}/${name}-${version}.0.tar.xz -O ${EXTERNALS_DOWNLOAD_DIR}/${name}-${version}.0.tar.xz
            DEPENDERS download
        )

    elseif(APPLE)

        # Extract MD5 sums of each KF5 components directly from server.
        execute_process(COMMAND
                            wget -q -O - -o /dev/null http://download.kde.org/stable/frameworks/${version}/${name}-${version}.0.tar.xz.mirrorlist
                        COMMAND
                            grep "MD5 Hash"
                        COMMAND
                            grep -o "<tt>.*</tt>"
                        COMMAND
                            cut -c 5-
                        COMMAND
                            rev
                        COMMAND
                            cut -c 6-
                        COMMAND
                            rev
                        RESULT_VARIABLE
                            wgetresult
                        OUTPUT_VARIABLE
                            md5sm
                        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(NOT ${wgetresult} MATCHES 0)
            message(WARNING "Cannot download MD5 sum for ${name}!")
        endif()

        message(STATUS "--------------------------------")
        message(STATUS "KF5 component    : ${name}")
        message(STATUS "Component version: ${version}")
        message(STATUS "Component MD5    : ${md5sm}")
        message(STATUS "Configure options: ${argoptions}")
        message(STATUS "Patch to apply   : ${patch}")

        if(NOT "${patch}" STREQUAL "")
            set(patch "${PATCH_COMMAND};-p1;-i;${CMAKE_CURRENT_SOURCE_DIR}/${patch};&&")
        endif()

        ExternalProject_Add(ext_${name}
                            DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
                            URL http://download.kde.org/stable/frameworks/${version}/${name}-${version}.0.tar.xz
                            URL_MD5 ${md5sm}

                            PATCH_COMMAND ${patch}
                                          cp ${CMAKE_CURRENT_SOURCE_DIR}/../../macports/fixbundledatapath.sh <SOURCE_DIR>/. &&
                                          <SOURCE_DIR>/fixbundledatapath.sh

                            CONFIGURE_COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/../../../../bootstrap.macports <SOURCE_DIR>/. &&
                                              sed -i '' -e "s/DBUILD_TESTING=ON/DBUILD_TESTING=OFF/g" <SOURCE_DIR>/bootstrap.macports &&
                                              <SOURCE_DIR>/bootstrap.macports ${EXTPREFIX_frameworks} debug x86_64 "${argoptions}"

                            BUILD_COMMAND cd <SOURCE_DIR>/build && $(MAKE)

                            INSTALL_COMMAND cd <SOURCE_DIR>/build && make install/fast

                            BUILD_IN_SOURCE 1

                            UPDATE_COMMAND ""
                            ALWAYS 0
        )

    else() # Linux

        # Extract MD5 sums of each KF5 components directly from server.
        execute_process(COMMAND
                        wget -q -O - -o /dev/null http://download.kde.org/stable/frameworks/${version}/${name}-${version}.0.tar.xz.mirrorlist
                        COMMAND
                        grep "MD5 Hash"
                        COMMAND
                        grep -o -P "(?<=<tt>).*(?=</tt>)"
                        RESULT_VARIABLE
                        wgetresult
                        OUTPUT_VARIABLE
                        md5sm
                        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(NOT ${wgetresult} MATCHES 0)
            message(WARNING "Cannot download MD5 sum for ${name}!")
        endif()

        message(STATUS "--------------------------------")
        message(STATUS "KF5 component    : ${name}")
        message(STATUS "Component version: ${version}")
        message(STATUS "Component MD5    : ${md5sm}")
        message(STATUS "Configure options: ${argoptions}")
        message(STATUS "Patch to apply   : ${patch}")

        if(NOT "${patch}" STREQUAL "")
            set(patch "${PATCH_COMMAND};-p1;-i;${CMAKE_CURRENT_SOURCE_DIR}/${patch}")
        endif()

        ExternalProject_Add(ext_${name}
                            DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
                            URL http://download.kde.org/stable/frameworks/${version}/${name}-${version}.0.tar.xz
                            URL_MD5 ${md5sm}

                            PATCH_COMMAND ${patch}

                            INSTALL_DIR ${EXTPREFIX_frameworks}
                            CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
                                       -DBUILD_TESTING=OFF
                                       "${argoptions}"
                                       -Wno-dev
                            UPDATE_COMMAND ""
                            ALWAYS 0
        )

    endif()

endfunction()

if(MINGW)
    set(KF5_VERSION 5.71) # we have Qt 5.14.2 available.
elseif(APPLE)
    set(KF5_VERSION 5.71) # we have Qt 5.14.1 available.
else()
    set(KF5_VERSION 5.71) # we have Qt 5.14.2 available.
endif()

if(APPLE)
    set(MacosThreadWeaverPatch threadweaver-macports.patch)
    set(MacosKauthBackend -DKAUTH_BACKEND_NAME=Apple)
endif()

InstallKF5Component(extra-cmake-modules ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kconfig             ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(breeze-icons        ${KF5_VERSION} "-DBINARY_ICONS_RESOURCE=OFF"                                        "${MacosBreezePatch}")
InstallKF5Component(solid               ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kcoreaddons         ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(threadweaver        ${KF5_VERSION} ""                                                                   "${MacosThreadWeaverPatch}")
InstallKF5Component(kwindowsystem       ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(karchive            ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kdbusaddons         ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(ki18n               ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kcrash              ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kpackage            ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kcodecs             ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kauth               ${KF5_VERSION} "${MacosKauthBackend}"                                               "")
InstallKF5Component(knewstuff           ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kguiaddons          ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kwidgetsaddons      ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kdeclarative        ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kitemviews          ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kcompletion         ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kconfigwidgets      ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kiconthemes         ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kservice            ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kglobalaccel        ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kxmlgui             ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kbookmarks          ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kdoctools          ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kimageformats       ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(knotifyconfig       ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(knotifications      ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kjobwidgets         ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(sonnet              ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(ktextwidgets        ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(attica                 ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(kio                 ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(frameworkintegration ${KF5_VERSION} ""                                                                   "")
InstallKF5Component(qqc2-desktop-style  ${KF5_VERSION} ""                                                                   "")

ExternalProject_Add(
  ext_kdecoration
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/plasma/5.18.3/kdecoration-5.18.3.tar.xz
  URL_MD5 150da856a2dab8e28f76011638f23869
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
)

ExternalProject_Add(
  ext_breeze
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/plasma/5.18.3/breeze-5.18.3.tar.xz
  URL_MD5 09018228efdfc18be6cc11092290b183
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kdecoration
)

#ExternalProject_Add(
#  ext_plasma-framework
#  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
#  URL http://download.kde.org/stable/frameworks/5.68/plasma-framework-5.68.0.zip
#  URL_MD5 dd2741b632ad0e7589e00ef39a605422
#  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
#  DEPENDS ext_kactivities
#)
