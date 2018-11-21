# this library isn't shipped with AppImage as per
# https://github.com/AppImage/AppImages/blob/master/excludelist#L126

ExternalProject_Add(
  ext_zlib
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://zlib.net/zlib-1.2.11.tar.gz
  URL_MD5 1c9f62f0778697a09d36121ead88e08e

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
