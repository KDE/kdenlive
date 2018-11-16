ExternalProject_Add(
  ext_postgresql
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://ftp.postgresql.org/pub/source/v11.0/postgresql-11.0.tar.bz2
  URL_MD5 338ede84c0443db02ff6e8c4c8cbc5e9

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} --with-libxml --with-libxslt --with-icu --with-tcl --enable-thread-safety --without-readline --with-openssl --with-libraries=${EXT_INSTALL_DIR}/lib --with-includes=${EXT_INSTALL_DIR}/include CPPFLAGS=${AT_CPP_FLAGS} LDFLAGS=${AT_LD_FLAGS}
  BUILD_COMMAND rsync -a --ignore-existing src/backend/utils/ src/include/utils/
  COMMAND       rsync -a --ignore-existing src/backend/catalog/ src/include/catalog/
  COMMAND $(MAKE) -C src/bin install
  COMMAND $(MAKE) -C src/include install
  COMMAND $(MAKE) -C src/interfaces install
  INSTALL_COMMAND ""
  UPDATE_COMMAND ""
  BUILD_IN_SOURCE 1
  DEPENDS ext_readline ext_tcl ext_xslt ext_openssl
)
