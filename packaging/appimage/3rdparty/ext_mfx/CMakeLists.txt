ExternalProject_Add(

  ext_mfx

  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY git://github.com/lu-zero/mfx_dispatch.git
  GIT_TAG master
  CONFIGURE_COMMAND autoreconf -fiv
  COMMAND ./configure --prefix=${EXT_INSTALL_DIR}
  BUILD_ALWAYS TRUE
  BUILD_IN_SOURCE 1
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
