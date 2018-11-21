# this library isn't shipped with AppImage as per
# https://github.com/AppImage/AppImages/blob/master/excludelist#L126

ExternalProject_Add(
  ext_harfbuzz
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://www.freedesktop.org/software/harfbuzz/release/harfbuzz-2.0.0.tar.bz2
  URL_MD5 6d1373dba3e8540c448c6a562f2e0a00

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --with-icu=yes --disable-gtk-doc-html
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
  DEPENDS ext_icu
)
