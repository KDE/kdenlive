ExternalProject_Add(
  ext_ruby
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://cache.ruby-lang.org/pub/ruby/2.7/ruby-2.7.2.tar.gz
  URL_MD5 2d4a28dcfa38352a627a597f6057c465

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
