ExternalProject_Add(
  ext_ladspa
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://github.com/swh/ladspa.git
  UPDATE_DISCONNECTED 1
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND <SOURCE_DIR>/autogen.sh --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
#  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR}
# TODO: ladspa.h header does not seem to be installed and should be manually copied
  BUILD_ALWAYS TRUE
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
