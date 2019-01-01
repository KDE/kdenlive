ExternalProject_Add(ext_gavl
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://downloads.sourceforge.net/project/gmerlin/gavl/1.4.0/gavl-1.4.0.tar.gz
  URL_MD5 2752013a817fbc43ddf13552215ec2c0

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --without-doxygen
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
