# --without-catalog is requirement of QWebEngine
ExternalProject_Add(
  ext_xml
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL ftp://xmlsoft.org/libxslt/libxml2-2.9.8.tar.gz
  URL_MD5 b786e353e2aa1b872d70d5d1ca0c740d

  PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/xml.diff

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} --without-python --with-icu --without-catalog --with-lzma=${EXT_INSTALL_DIR}/lib CPPFLAGS=${AT_CPP_FLAGS} LDFLAGS=${AT_LD_FLAGS}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
  DEPENDS ext_lzma ext_readline ext_icu
)
