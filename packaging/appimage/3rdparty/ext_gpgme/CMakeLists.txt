ExternalProject_Add(
  ext_gpgme
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://www.gnupg.org/ftp/gcrypt/gpgme/gpgme-1.12.0.tar.bz2
  URL_MD5 902fca3a94907efe4e929b2ade545a7c

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --enable-languages=cpp,qt --disable-gpg-test
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_assuan2
)
