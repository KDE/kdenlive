ExternalProject_Add(
  ext_boost
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://boostorg.jfrog.io/artifactory/main/release/1.68.0/source/boost_1_68_0.tar.bz2
  URL_MD5 7fbd1890f571051f2a209681d57d486a

  CONFIGURE_COMMAND <SOURCE_DIR>/bootstrap.sh --prefix=${EXT_INSTALL_DIR} --with-libraries=system
  BUILD_COMMAND <SOURCE_DIR>/b2 -j1 install
  INSTALL_COMMAND ""

  BUILD_IN_SOURCE 1
)
