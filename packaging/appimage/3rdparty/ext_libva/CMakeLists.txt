ExternalProject_Add(
  ext_libva
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://github.com/intel/libva.git

  CONFIGURE_COMMAND <SOURCE_DIR>/autogen.sh && <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_IN_SOURCE 1
  BUILD_ALWAYS TRUE
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
