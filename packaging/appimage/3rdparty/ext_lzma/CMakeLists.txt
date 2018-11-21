ExternalProject_Add(
  ext_lzma
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://tukaani.org/xz/xz-5.2.4.tar.xz
  URL_MD5 003e4d0b1b1899fc6e3000b24feddf7c

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} --disable-doc
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
