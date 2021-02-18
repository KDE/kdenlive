ExternalProject_Add(
  ext_webp
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.1.0.tar.gz
  URL_MD5 7e047f2cbaf584dff7a8a7e0f8572f18
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
