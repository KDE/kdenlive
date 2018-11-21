ExternalProject_Add(
  ext_icu
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.icu-project.org/files/icu4c/63.1/icu4c-63_1-src.tgz
  URL_MD5 9e40f6055294284df958200e308bce50

  CONFIGURE_COMMAND <SOURCE_DIR>/source/configure --prefix=${EXT_INSTALL_DIR} --disable-tests
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
