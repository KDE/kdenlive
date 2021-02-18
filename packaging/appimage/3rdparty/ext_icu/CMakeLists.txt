ExternalProject_Add(
  ext_icu
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://github.com/unicode-org/icu/archive/release-64-2.tar.gz
  URL_MD5 b52fcb73e71545c6528beadc755e45e7
  CONFIGURE_COMMAND cd <SOURCE_DIR>/icu4c/source &&
                       ./configure --prefix ${EXT_INSTALL_DIR}/local
                                   --enable-tests=no
                                   --enable-samples=no
                                   --with-data-packaging=library
                                   --disable-renaming
  BUILD_COMMAND cd <SOURCE_DIR>/icu4c/source && $(MAKE)
  INSTALL_COMMAND cd <SOURCE_DIR>/icu4c/source && $(MAKE) install
  UPDATE_COMMAND ""
  BUILD_IN_SOURCE 1
  ALWAYS 0
)
