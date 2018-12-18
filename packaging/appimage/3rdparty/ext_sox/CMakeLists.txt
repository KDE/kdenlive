ExternalProject_Add(
  ext_sox
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://downloads.sourceforge.net/project/sox/sox/14.4.2/sox-14.4.2.tar.gz
  URL_MD5 d04fba2d9245e661f245de0577f48a33

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
