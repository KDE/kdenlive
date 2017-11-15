# Script to build Qt for digiKam bundle.
#
# Copyright (c) 2015-2017, Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

SET(EXTPREFIX_qt "${EXTPREFIX}")

ExternalProject_Add(ext_qt
    DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
    URL https://download.qt.io/official_releases/qt/5.9/5.9.2/single/qt-everywhere-opensource-src-5.9.2.tar.xz
    URL_MD5 738d1b98106e1bd39f00cc228beb522a
    CONFIGURE_COMMAND <SOURCE_DIR>/configure
                  -prefix ${EXTPREFIX_qt}
                  -opensource
                  -confirm-license
                  -nomake examples
                  -sql-sqlite
                  -fontconfig
                  -openssl
                  -optimized-qmake
                  -no-mtdev
                  -no-journald
                  -no-syslog
                  -no-tslib
                  -no-directfb
                  -no-linuxfb
                  -no-libproxy
                  -no-pch
                  -no-compile-examples
                  -qt-zlib
                  -qt-pcre
                  -qt-harfbuzz
                  -qt-xcb
                  -qt-xkbcommon-x11
                  -skip qt3d
                  -skip qtactiveqt
                  -skip qtandroidextras
                  -skip qtcanvas3d
                  -skip qtcharts
                  -skip qtconnectivity
                  -skip qtdatavis3d
                  -skip qtdoc
                  -skip qtgamepad
                  -skip qtlocation
                  -skip qtmultimedia
                  -skip qtpurchasing
                  -skip qtremoteobjects
                  -skip qtserialport
                  -skip qtscxml
                  -skip qtsensors
                  -skip qtspeech
                  -skip qttranslations
                  -skip qtvirtualkeyboard
                  -skip qtwayland
                  -skip qtwebsockets
                  -skip qtwebchannel
                  -skip qtwebengine

    UPDATE_COMMAND ""
    BUILD_IN_SOURCE 1
    ALWAYS 0
)
