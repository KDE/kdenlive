ExternalProject_Add(
  ext_yasm
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY git://github.com/yasm/yasm.git
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
)
