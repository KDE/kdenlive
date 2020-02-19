ExternalProject_Add(
  ext_rubberband
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY git://github.com/breakfastquay/rubberband.git
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} --disable-programs --disable-vamp --disable-ladspa
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

)
