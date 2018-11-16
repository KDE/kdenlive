ExternalProject_Add(
  ext_gdkpixbuf
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://ftp.gnome.org/pub/GNOME/sources/gdk-pixbuf/2.23/gdk-pixbuf-2.23.5.tar.xz
  URL_MD5 066e30277cadc225a7cef4c258da830b

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_alsa
)
