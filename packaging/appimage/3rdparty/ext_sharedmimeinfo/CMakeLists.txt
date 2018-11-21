ExternalProject_Add(
  ext_sharedmimeinfo
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://people.freedesktop.org/~hadess/shared-mime-info-1.10.tar.xz
  URL_MD5 418c2ced9dc4dd5ca8b06a755e6d64e9

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} ${AUTOMAKE_DEFAULT_HOST} ${AUTOMAKE_SHARED_ONLY} --disable-update-mimedb
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_intltool
)
