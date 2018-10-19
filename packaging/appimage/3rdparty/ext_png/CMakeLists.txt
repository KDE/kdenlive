# Script to build libpng for digiKam bundle.
#
# Copyright (c) 2015-2017, Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

SET(PREFIX_ext_png "${EXTPREFIX}")

ExternalProject_Add(ext_png
    DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
    URL http://prdownloads.sourceforge.net/libpng/libpng-1.6.26.tar.xz
    URL_MD5 faed9bb495d2e12dd0c9ec561ca60cd8

    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${PREFIX_ext_png}
    UPDATE_COMMAND ""
    ALWAYS 0
)
