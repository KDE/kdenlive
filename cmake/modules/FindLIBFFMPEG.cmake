#
# Find the FFMPEG includes and library
#

FIND_PROGRAM(FFMPEG_CONFIG_EXECUTABLE pkg-config)
EXEC_PROGRAM(${FFMPEG_CONFIG_EXECUTABLE} ARGS --variable=includedir libavformat OUTPUT_VARIABLE FFMPEG_HEADER_PATH )
#MESSAGE(STATUS "Found FFmpeg header pkg-config: ${FFMPEG_CONFIG_EXECUTABLE} , ${FFMPEG_HEADER_PATH}")
EXEC_PROGRAM(${FFMPEG_CONFIG_EXECUTABLE} ARGS --variable=libdir libavformat OUTPUT_VARIABLE FFMPEG_LIBS_PATH )
#MESSAGE(STATUS "Found FFmpeg lib pkg-config: ${FFMPEG_CONFIG_EXECUTABLE} , ${FFMPEG_LIBS_PATH}")

FIND_PATH(LIBFFMPEG_INCLUDE_DIR 
  NAMES avformat.h
  PATHS ${FFMPEG_HEADER_PATH}/ffmpeg ${FFMPEG_HEADER_PATH}/libavformat ${FFMPEG_HEADER_PATH}/ffmpeg/libavformat
  /usr/include/ffmpeg /usr/include/libavformat /usr/include/ffmpeg/libavformat
  /usr/local/include/ffmpeg /usr/local/include/libavformat /usr/local/include/ffmpeg/libavformat
  NO_DEFAULT_PATH
)

IF (LIBFFMPEG_INCLUDE_DIR)
  MESSAGE(STATUS "Found FFmpeg Libavformat includes: ${LIBFFMPEG_INCLUDE_DIR}")
ELSE (LIBFFMPEG_INCLUDE_DIR)
  MESSAGE(FATAL_ERROR "\n***************************\n"
    "Could not find FFmpeg Libavformat includes\n"
    "please install the libavformat-dev package or give the path manually using\n"
    "-DLIBFFMPEG_INCLUDE_DIR=PATH_TO_YOUR_AVFORMAT_HEADERS"
    "\n***************************\n")
ENDIF (LIBFFMPEG_INCLUDE_DIR)

FIND_LIBRARY(LIBFFMPEG_LIBRARY
  NAMES avformat
  PATHS ${FFMPEG_LIBS_PATH}
  /usr/lib /usr/local/lib
  NO_DEFAULT_PATH
)

IF (LIBFFMPEG_LIBRARY)
  MESSAGE(STATUS "Found FFmpeg Libavformat library: ${LIBFFMPEG_LIBRARY}")
ELSE (LIBFFMPEG_LIBRARY)
  MESSAGE(FATAL_ERROR "\n***************************\n"
    "Could not find FFmpeg Libavformat library\n"
    "please install the libavformat package or give the path manually using\n"
    "-DLIBFFMPEG_LIBRARY=PATH_TO_YOUR_AVFORMAT_LIBRARY"
    "\n***************************\n")
ENDIF (LIBFFMPEG_LIBRARY)

IF (LIBFFMPEG_LIBRARY AND LIBFFMPEG_INCLUDE_DIR)
  SET( LIBFFMPEG_FOUND 1 )
  SET( LIBFFMPEG_LIBRARIES ${LIBFFMPEG_LIBRARY} )
ELSE (LIBFFMPEG_LIBRARY AND LIBFFMPEG_INCLUDE_DIR)
  SET( LIBFFMPEG_FOUND 0 )
ENDIF (LIBFFMPEG_LIBRARY AND LIBFFMPEG_INCLUDE_DIR)


