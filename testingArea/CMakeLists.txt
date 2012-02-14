
message(STATUS "Building experimental executables")

include_directories(
  ${LIBMLT_INCLUDE_DIR}
  ${LIBMLTPLUS_INCLUDE_DIR}
)
include(${QT_USE_FILE})

add_executable(audioOffset
    audioOffset.cpp
    ../src/lib/audio/audioInfo.cpp
    ../src/lib/audio/audioStreamInfo.cpp
    ../src/lib/audio/audioEnvelope.cpp
    ../src/lib/audio/audioCorrelation.cpp
    ../src/lib/audio/audioCorrelationInfo.cpp
)
target_link_libraries(audioOffset 
  ${QT_LIBRARIES}
  ${LIBMLT_LIBRARY}
  ${LIBMLTPLUS_LIBRARY}
)
