ExternalProject_Add(
  ext_ncurses
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.1.tar.gz
  URL_MD5 98c889aaf8d23910d2b92d65be2e737a

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} --enable-overwrite --enable-pc-files --without-tests --with-shared --with-cxx-shared --without-ada --with-pkg-config-libdir=${EXT_INSTALL_DIR}/lib/pkgconfig
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
