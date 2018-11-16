ExternalProject_Add(
  ext_pcre2
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://ftp.pcre.org/pub/pcre/pcre2-10.32.zip
  URL_MD5 e0e5b1f7e425b73dcdcea0e08a85da31

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --enable-unicode --enable-pcre2-16 --enable-pcre2-32 --enable-jit
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
