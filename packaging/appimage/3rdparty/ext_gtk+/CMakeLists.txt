ExternalProject_Add(
  ext_gtk+
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://ftp.gnome.org/pub/GNOME/sources/gtk+/2.23/gtk+-2.23.90.tar.gz
  URL_MD5 404c75655bc497994c13a1aa92c9949a

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} --disable-cups
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_alsa
)
