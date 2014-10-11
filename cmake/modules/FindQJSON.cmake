# Find QJSON - JSON handling library for Qt
#
# This module defines
#  QJSON_FOUND - whether the qsjon library was found
#  QJSON_LIBRARIES - the qjson library
#  QJSON_INCLUDE_DIR - the include path of the qjson library
#
# Copyright (c) 2010 Pino Toscano, <toscano.pino@tiscali.it>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.



if (QJSON_INCLUDE_DIR AND QJSON_LIBRARIES)

  # Already in cache
  set (QJSON_FOUND TRUE)

else (QJSON_INCLUDE_DIR AND QJSON_LIBRARIES)

  if (NOT WIN32)
    # use pkg-config to get the values of QJSON_INCLUDE_DIRS
    # and QJSON_LIBRARY_DIRS to add as hints to the find commands.
    include (FindPkgConfig)
    pkg_check_modules (QJSON REQUIRED QJson>=0.5)
  endif (NOT WIN32)

  find_library (QJSON_LIBRARIES
    NAMES
    qjson
    PATHS
    ${QJSON_LIBRARY_DIRS}
    ${LIB_INSTALL_DIR}
    ${KDE4_LIB_DIR}
  )

  find_path (QJSON_INCLUDE_DIR
    NAMES
    qjson/parser.h
    PATHS
    ${QJSON_INCLUDE_DIRS}
    ${INCLUDE_INSTALL_DIR}
    ${KDE4_INCLUDE_DIR}
  )

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(QJSON DEFAULT_MSG QJSON_LIBRARIES QJSON_INCLUDE_DIR)

endif (QJSON_INCLUDE_DIR AND QJSON_LIBRARIES)
