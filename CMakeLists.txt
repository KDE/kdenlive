SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmakemodules ${CMAKE_SOURCE_DIR}/cmake/modules)

# INCLUDE(FindKdeLibs)
INCLUDE(FindGettext)
INCLUDE(KdenlivePackageMessages)

# START MLT
SET(LIBMLT_DIR ${CMAKE_MODULE_PATH})
FIND_PACKAGE(LIBMLT REQUIRED)
IF(LIBMLT_FOUND)
  SET(HAVE_LIBMLT 1)
  SET(LIBMLT_INC "mlt.h")
  MESSAGE("MLT Library Found OK")
ENDIF(LIBMLT_FOUND)
# END LIBMLT


#ADD_DEFINITIONS ( CXXFLAGS="$CXXFLAGS -DMLT_PREFIX=\\\"\"MLT_PATH\"\\\"" )
#SET(CMAKE_C_FLAGS `mlt-config --cflags`)
INCLUDE_DIRECTORIES(
${LIBMLT_INCLUDE_DIR}
${LIBMLT_INCLUDE_DIR}/mlt
#/usr/local/include/mlt
)

SUBDIRS(kdenlive renderer graphics profiles pgm icons doc po effects)

