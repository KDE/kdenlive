ExternalProject_Add(
  ext_intltool
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://launchpad.net/intltool/trunk/0.51.0/+download/intltool-0.51.0.tar.gz
  URL_MD5 12e517cac2b57a0121cda351570f1e63

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
