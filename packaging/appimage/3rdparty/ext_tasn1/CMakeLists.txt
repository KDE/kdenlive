ExternalProject_Add(
  ext_tasn1
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://ftp.gnu.org/gnu/libtasn1/libtasn1-4.13.tar.gz
  URL_MD5 ce2ba4d3088119b48e7531a703669c52

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
