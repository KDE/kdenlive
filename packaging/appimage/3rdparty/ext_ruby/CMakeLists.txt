ExternalProject_Add(
  ext_ruby
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://cache.ruby-lang.org/pub/ruby/2.4/ruby-2.4.5.tar.gz
  URL_MD5 4e6b030af3897b811059e1289fc75b31

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
