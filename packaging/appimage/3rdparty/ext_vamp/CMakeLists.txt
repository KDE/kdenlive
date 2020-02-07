ExternalProject_Add(
  ext_vamp
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://code.soundsoftware.ac.uk/attachments/download/2588/vamp-plugin-sdk-2.9.0.tar.gz
  URL_MD5 b390743be58c68e0ab2de833c3dbc378
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} 
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

)
