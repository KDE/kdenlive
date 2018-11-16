ExternalProject_Add( ext_png
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://kent.dl.sourceforge.net/project/libpng/libpng16/1.6.35/libpng-1.6.35.tar.xz
  URL_MD5 678b7e696a62a193ed3503b04bf449d6

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
             -DPNG_TESTS=OFF
)
