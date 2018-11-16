ExternalProject_Add(
  ext_alsa
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL ftp://ftp.alsa-project.org/pub/lib/alsa-lib-1.1.5.tar.bz2
  URL_MD5 a2b465c3a5265d8a57f3ff39c6c4fc29

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

)
