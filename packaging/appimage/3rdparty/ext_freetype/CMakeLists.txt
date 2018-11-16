# this library isn't shipped with AppImage as per
# https://github.com/AppImage/AppImages/blob/master/excludelist#L126

ExternalProject_Add(
  ext_freetype
	DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
	URL https://quantum-mirror.hu/mirrors/pub/gnusavannah/freetype/freetype-2.9.1.tar.bz2
  URL_MD5 60ef7d8160cd4bf8cb118ee9d65367ca

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --enable-freetype-config
  BUILD_ALWAYS TRUE
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
