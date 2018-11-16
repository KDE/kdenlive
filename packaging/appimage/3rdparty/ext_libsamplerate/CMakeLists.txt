ExternalProject_Add(
  ext_libsamplerate
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://www.mega-nerd.com/SRC/libsamplerate-0.1.9.tar.gz
  URL_MD5 2b78ae9fe63b36b9fbb6267fad93f259

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

)
