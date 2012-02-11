
message(STATUS "Building experimental executables")

include_directories(
  ${LIBMLT_INCLUDE_DIR}
  ${LIBMLTPLUS_INCLUDE_DIR}
)
include(${QT_USE_FILE})

add_executable(audioOffset audioOffset.cpp)
target_link_libraries(audioOffset 
  ${QT_LIBRARIES}
  ${LIBMLT_LIBRARY}
  ${LIBMLTPLUS_LIBRARY}
)
