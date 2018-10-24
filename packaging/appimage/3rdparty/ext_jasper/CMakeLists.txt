# Script to build jasperlib for digiKam bundle.
#
# Copyright (c) 2015-2017, Gilles Caulier, <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

SET(PREFIX_ext_jasper "${EXTPREFIX}")

ExternalProject_Add(ext_jasper
    DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
    URL http://www.ece.uvic.ca/~frodo/jasper/software/jasper-1.900.29.tar.gz
    URL_MD5 4619ec9860c10e557b3f192f5e76f596

    CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${PREFIX_ext_jasper} ${GLOBAL_AUTOMAKE_PROFILE} --disable-opengl
    UPDATE_COMMAND ""
    ALWAYS 0
)
