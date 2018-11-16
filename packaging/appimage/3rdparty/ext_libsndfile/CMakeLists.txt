ExternalProject_Add(
  ext_libsndfile
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://www.mega-nerd.com/libsndfile/files/libsndfile-1.0.28.tar.gz
  URL_MD5 646b5f98ce89ac60cdb060fcd398247c

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

)
