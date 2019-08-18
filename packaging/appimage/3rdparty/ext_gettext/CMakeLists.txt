ExternalProject_Add(
  ext_gettext
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://ftp.gnu.org/pub/gnu/gettext/gettext-0.19.8.1.tar.gz
  URL_MD5 97e034cf8ce5ba73a28ff6c3c0638092

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --disable-java --disable-native-java --enable-nls --enable-c++
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_xml ext_ncurses
)
