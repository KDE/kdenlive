if(BUILD_JogShuttle)
  # Create a static library media_ctrl
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")
  include_directories(
    ${CMAKE_BINARY_DIR}
  )
  add_library(media_ctrl STATIC
    mediactrl.c
  )
endif(BUILD_JogShuttle)
