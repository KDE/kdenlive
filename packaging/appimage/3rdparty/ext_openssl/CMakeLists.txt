ExternalProject_Add(
  ext_openssl
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://www.openssl.org/source/openssl-1.1.1.tar.gz
  URL_MD5 7079eb017429e0ffb9efb42bf80ccb21

  CONFIGURE_COMMAND <SOURCE_DIR>/config --prefix=${EXT_INSTALL_DIR}/openssl --openssldir=${EXT_INSTALL_DIR}/openssl/ssl
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install_sw
)
