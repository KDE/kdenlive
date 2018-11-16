ExternalProject_Add(
  ext_pango
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://ftp.gnome.org/pub/GNOME/sources/pango/1.28/pango-1.28.4.tar.gz
  URL_MD5 97cab905b280ecfffdbb423a98aedf39

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

)
