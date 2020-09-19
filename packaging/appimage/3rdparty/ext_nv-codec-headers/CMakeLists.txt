ExternalProject_Add(                                                                                                                                                       
  ext_nv-codec-headers                                                                                                                                                               
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}                                                                                                                                         
  GIT_REPOSITORY git://github.com/FFmpeg/nv-codec-headers.git
  GIT_TAG master
  CONFIGURE_COMMAND sed -i s,/usr/local,${EXT_INSTALL_DIR}, <SOURCE_DIR>/Makefile
  BUILD_ALWAYS TRUE
  BUILD_IN_SOURCE 1
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
