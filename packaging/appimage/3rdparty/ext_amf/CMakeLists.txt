ExternalProject_Add(

  ext_amf

  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/AMF.git
  GIT_TAG master
  CONFIGURE_COMMAND rm -rf <SOURCE_DIR>/Thirdparty
  BUILD_ALWAYS TRUE
  BUILD_IN_SOURCE 1
  BUILD_COMMAND mkdir -p ${EXT_INSTALL_DIR}/include/AMF
  INSTALL_COMMAND cp -av <SOURCE_DIR>/amf/public/include/. ${EXT_INSTALL_DIR}/include/AMF
)
