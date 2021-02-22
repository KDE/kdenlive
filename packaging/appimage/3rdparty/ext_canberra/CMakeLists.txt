ExternalProject_Add(
  ext_canberra
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://0pointer.de/lennart/projects/libcanberra/libcanberra-0.30.tar.xz
  URL_MD5 34cb7e4430afaf6f447c4ebdb9b42072

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_IN_SOURCE 1
  BUILD_ALWAYS TRUE
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
