ExternalProject_Add(
  ext_lame
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://sourceforge.net/projects/lame/files/lame/3.100/lame-3.100.tar.gz/download
  URL_MD5 83e260acbe4389b54fe08e0bdbf7cddb

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_IN_SOURCE 1
  BUILD_ALWAYS TRUE
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
