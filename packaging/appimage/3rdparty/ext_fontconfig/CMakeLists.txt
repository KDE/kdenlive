ExternalProject_Add(
  ext_fontconfig
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://files.kde.org/krita/build/dependencies/fontconfig-2.12.0.tar.gz
  URL_MD5 d8b056231abcb6257db6dc6d745360b2

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
