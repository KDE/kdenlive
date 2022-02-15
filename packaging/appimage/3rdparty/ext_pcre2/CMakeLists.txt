ExternalProject_Add(
  ext_pcre2
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://github.com/PhilipHazel/pcre2/releases/download/pcre2-10.39/pcre2-10.39.zip
  URL_MD5 e101c0ca9edb4b0af103bebe78ba52b0

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --enable-unicode --enable-pcre2-16 --enable-pcre2-32 --enable-jit
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
