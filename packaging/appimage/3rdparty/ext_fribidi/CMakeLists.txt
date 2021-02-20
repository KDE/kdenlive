ExternalProject_Add(
  ext_fribidi
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://github.com/fribidi/fribidi/releases/download/v1.0.10/fribidi-1.0.10.tar.xz
  URL_MD5 97c87da9930e8e70fbfc8e2bcd031554

  CONFIGURE_COMMAND <SOURCE_DIR>/autogen.sh && <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY}
  BUILD_IN_SOURCE 1
  BUILD_ALWAYS TRUE
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
