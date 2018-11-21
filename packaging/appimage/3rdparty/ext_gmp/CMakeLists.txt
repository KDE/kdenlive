ExternalProject_Add(
  ext_gmp
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://gmplib.org/download/gmp/gmp-6.1.2.tar.bz2
  URL_MD5 8ddbb26dc3bd4e2302984debba1406a5

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --enable-cxx
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
