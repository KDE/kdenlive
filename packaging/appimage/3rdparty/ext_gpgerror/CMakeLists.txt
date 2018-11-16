ExternalProject_Add(
  ext_gpgerror
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://www.gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-1.32.tar.bz2
  URL_MD5 ef3d928a5a453fa701ecc3bb22be1c64

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
