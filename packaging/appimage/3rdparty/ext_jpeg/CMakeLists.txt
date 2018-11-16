ExternalProject_Add(
  ext_jpeg
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://downloads.sourceforge.net/libjpeg-turbo/libjpeg-turbo-2.0.0.tar.gz
  URL_MD5 b12a3fcf1d078db38410f27718a91b83

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
             -DENABLE_STATIC=OFF

  DEPENDS ext_nasm
)
