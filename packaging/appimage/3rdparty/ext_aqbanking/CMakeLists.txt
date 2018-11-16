ExternalProject_Add(
  ext_aqbanking
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://www.aquamaniac.de/sites/download/download.php?package=03&release=217&file=02&dummy=aqbanking-5.7.8.tar.gz
  URL_MD5 6aceeaad3a1000f22f08986e68946db3

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} CPPFLAGS=${AT_CPP_FLAGS} LDFLAGS=${AT_LD_FLAGS}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_gwenhywfar ext_ktoblzcheck ext_xmlsec1 ext_gmp
)
