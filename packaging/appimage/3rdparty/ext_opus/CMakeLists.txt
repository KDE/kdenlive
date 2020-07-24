ExternalProject_Add(
  ext_opus
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz
  URL_MD5 d7c07db796d21c9cf1861e0c2b0c0617

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
