ExternalProject_Add(
  ext_ogg
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://ftp.osuosl.org/pub/xiph/releases/ogg/libogg-1.3.5.tar.gz
  URL_HASH SHA256=0eb4b4b9420a0f51db142ba3f9c64b333f826532dc0f48c6410ae51f4799b664

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_IN_SOURCE 1
  BUILD_ALWAYS TRUE
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
