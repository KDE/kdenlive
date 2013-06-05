
message(STATUS "Building experimental executables")

include_directories(
  ${CMAKE_BINARY_DIR}
  ${LIBMLT_INCLUDE_DIR}
  ${LIBMLTPLUS_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/src/nolib/extern/kiss_fft
  ${PROJECT_SOURCE_DIR}/src/nolib/extern/kiss_fft/tools
)
include(${QT_USE_FILE})

add_executable(audioOffset
    audioOffset.cpp
    ../src/nolib/audio/audioInfo.cpp
    ../src/nolib/audio/audioStreamInfo.cpp
    ../src/nolib/audio/audioEnvelope.cpp
    ../src/nolib/audio/audioCorrelation.cpp
    ../src/nolib/audio/audioCorrelationInfo.cpp
    ../src/nolib/audio/fftCorrelation.cpp
)
target_link_libraries(audioOffset 
  ${QT_LIBRARIES}
  ${LIBMLT_LIBRARY}
  ${LIBMLTPLUS_LIBRARY}
  kiss_fft
)
