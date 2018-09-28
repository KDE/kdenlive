# cmake macro to find DrMinGW Windows crash handler
#
# copyright (c) 2018, Vincent Pinon <vpinon@kde.org>
#
# once done this will define:
#
#  DRMINGW_FOUND - system has DrMinGW
#  DRMINGW_INCLUDE_DIR - the DrMinGW include directory
#  DRMINGW_LIBRARY - the libraries needed to use DrMinGW
#
# redistribution and use is allowed according to the terms of the bsd license.

if (DRMINGW_INCLUDE_DIR AND DRMINGW_LIBRARY)
    # already in cache, be silent
    set (DRMINGW_FIND_QUIETLY true)
endif (DRMINGW_INCLUDE_DIR AND DRMINGW_LIBRARY)

find_path (DRMINGW_INCLUDE_DIR exchndl.h)
find_library (DRMINGW_LIBRARY exchndl)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (DrMinGW DEFAULT_MSG DRMINGW_INCLUDE_DIR DRMINGW_LIBRARY)

mark_as_advanced(DRMINGW_INCLUDE_DIR DRMINGW_LIBRARY)

