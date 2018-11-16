ExternalProject_Add(
  ext_readline
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://ftp.gnu.org/gnu/readline/readline-7.0.tar.gz
  URL_MD5 205b03a87fc83dab653b628c59b9fc91

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --with-curses
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_ncurses
)
