ExternalProject_Add(
  ext_xslt
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL ftp://xmlsoft.org/libxslt/libxslt-1.1.32.tar.gz
  URL_MD5 1fc72f98e98bf4443f1651165f3aa146

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_xml
)
