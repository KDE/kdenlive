ExternalProject_Add(
  ext_cmake
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://gitlab.kitware.com/cmake/cmake.git
  GIT_TAG v3.18.2
  CONFIGURE_COMMAND cd <SOURCE_DIR> && ./bootstrap --prefix=${EXT_INSTALL_DIR}
  BUILD_COMMAND ${MAKE}
  UPDATE_COMMAND ""
  BUILD_IN_SOURCE 1
  ALWAYS 0
)
