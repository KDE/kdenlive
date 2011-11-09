# Check src/CMakeLists.txt for detailed comments on the contents of this file.

set(kdenlive_render_SRCS
  kdenlive_render.cpp
  renderjob.cpp
)

kde4_add_executable(kdenlive_render ${kdenlive_render_SRCS})

include_directories(
  ${QT_INCLUDES}
)

include(${QT_USE_FILE})

target_link_libraries(kdenlive_render
  ${QT_LIBRARIES}
)

install(TARGETS kdenlive_render DESTINATION ${BIN_INSTALL_DIR})
