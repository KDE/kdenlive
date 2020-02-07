ExternalProject_Add(
# We have to comment out line 186 of Makefile.in to disable copying of jni file or install will fail

  ext_rubberband
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://breakfastquay.com/files/releases/rubberband-1.8.2.tar.bz2
  URL_MD5 db0ecb4f1a647bdaf7e43ef2ca2f7883
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

)
