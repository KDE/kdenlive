ExternalProject_Add(
  ext_iconv
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.15.tar.gz
  URL_MD5 ace8b5f2db42f7b3b3057585e80d9808

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --enable-relocatable
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
