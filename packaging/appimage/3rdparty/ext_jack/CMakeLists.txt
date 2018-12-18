ExternalProject_Add(
  ext_jack
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://github.com/jackaudio/jack2/releases/download/v1.9.12/jack2-1.9.12.tar.gz
  URL_MD5 6cb5dfea0586bcf009c733c4e4b04a03

  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND ./waf configure --prefix=${EXT_INSTALL_DIR}
  BUILD_ALWAYS TRUE
  BUILD_COMMAND ./waf build
  INSTALL_COMMAND ./waf install
)
