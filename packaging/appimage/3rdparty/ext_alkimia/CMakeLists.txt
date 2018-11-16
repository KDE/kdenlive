ExternalProject_Add(
  ext_alkimia
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://download.kde.org/stable/alkimia/7.0.2/alkimia-7.0.2.tar.xz
  URL_MD5 d826e092f56e528c70c00748a04e4957

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_gmp
)
