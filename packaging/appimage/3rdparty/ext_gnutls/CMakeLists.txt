ExternalProject_Add(
  ext_gnutls
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://www.gnupg.org/ftp/gcrypt/gnutls/v3.6/gnutls-3.6.4.tar.xz
  URL_MD5 63363d1c00601f4d11a5cadc8b5e0799

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --disable-tests --disable-doc --enable-openssl-compatibility -disable-gtk-doc --enable-cxx --without-p11-kit CPPFLAGS=${AT_CPP_FLAGS} LDFLAGS=${AT_LD_FLAGS}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_assuan2 ext_gpgerror ext_nettle ext_tasn1 ext_unistring
)
