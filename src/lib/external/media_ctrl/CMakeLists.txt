set(kdenlive_SRCS
  ${kdenlive_SRCS}
  lib/external/media_ctrl/mediactrl.h
  lib/external/media_ctrl/mediactrl.c
  PARENT_SCOPE
)


# Create a static library from media_ctrl
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")
#include_directories(
#  ${CMAKE_BINARY_DIR}
#)
#add_library(media_ctrl STATIC
#  mediactrl.h
#  mediactrl.c
#)
