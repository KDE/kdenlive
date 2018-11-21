ExternalProject_Add(
  ext_cairo
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://www.cairographics.org/releases/cairo-1.14.10.tar.xz
  URL_MD5 146f5f4d0b4439fc3792fd3452b7b12a

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} 
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

)
