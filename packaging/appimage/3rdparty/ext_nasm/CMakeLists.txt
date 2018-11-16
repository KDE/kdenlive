ExternalProject_Add(
  ext_nasm
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://www.nasm.us/pub/nasm/releasebuilds/2.13.03/nasm-2.13.03.tar.xz
  URL_MD5 d5ca2ad7121ccbae69dd606b1038532c

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
