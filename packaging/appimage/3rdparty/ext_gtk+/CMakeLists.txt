ExternalProject_Add(
  ext_gtk+
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://ftp.gnome.org/pub/GNOME/sources/gtk+/2.24/gtk+-2.24.32.tar.xz
  URL_MD5 d5742aa42275203a499b59b4c382a784

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} --disable-cups
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_alsa
)
