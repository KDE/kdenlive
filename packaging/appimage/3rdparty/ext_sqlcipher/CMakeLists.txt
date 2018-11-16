ExternalProject_Add(
  ext_sqlcipher
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://github.com/sqlcipher/sqlcipher/archive/v3.4.2.zip
  URL_MD5 f12c5975d784856ea89f83cfc9af1a6c

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} CPPFLAGS=${AT_CPP_FLAGS} LDFLAGS=${AT_LD_FLAGS} --enable-tempstore=yes CFLAGS=-DSQLITE_HAS_CODEC
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_tcl
)
