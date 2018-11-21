ExternalProject_Add(
  ext_opensp
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://downloads.sourceforge.net/project/openjade/opensp/1.5.2/OpenSP-1.5.2.tar.gz
  URL_MD5 670b223c5d12cee40c9137be86b6c39b

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --disable-doc-build
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
