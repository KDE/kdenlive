ExternalProject_Add(
  ext_ogg
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://downloads.xiph.org/releases/ogg/libogg-1.3.4.tar.gz
  URL_HASH SHA256=fe5670640bd49e828d64d2879c31cb4dde9758681bb664f9bdbf159a01b0c76e

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_IN_SOURCE 1
  BUILD_ALWAYS TRUE
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
