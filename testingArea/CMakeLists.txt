
message(STATUS "Building experimental executables")

include_directories(
  ${CMAKE_BINARY_DIR}
  ${MLT_INCLUDE_DIR}
  ${MLTPP_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/src/lib/extern/kiss_fft
  ${PROJECT_SOURCE_DIR}/src/lib/extern/kiss_fft/tools
)
include(${QT_USE_FILE})

add_executable(audioOffset
    audioOffset.cpp
    ../src/lib/audio/audioInfo.cpp
    ../src/lib/audio/audioStreamInfo.cpp
    ../src/lib/audio/audioEnvelope.cpp
    ../src/lib/audio/audioCorrelation.cpp
    ../src/lib/audio/audioCorrelationInfo.cpp
    ../src/lib/audio/fftCorrelation.cpp
)
target_link_libraries(audioOffset 
  ${QT_LIBRARIES}
  ${MLT_LIBRARIES}
  ${MLTPP_LIBRARIES}
  kiss_fft
)
