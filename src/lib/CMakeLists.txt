add_subdirectory(audio)
add_subdirectory(external)
set(kdenlive_SRCS
  ${kdenlive_SRCS}
  lib/qtimerWithTime.cpp
  PARENT_SCOPE)

