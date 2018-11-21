# aqbanking checks only for xmlsec1-gnutls >= 1.0.0 so we don't need openssl and nss
# we need gcrypt though because xmlsec-gnutls library requires xmlsec-gcrypt
ExternalProject_Add(
  ext_xmlsec1
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://www.aleksey.com/xmlsec/download/xmlsec1-1.2.26.tar.gz
  URL_MD5 9c4aaf9ff615a73921b9e3bf4988d878

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --without-openssl --without-nss --disable-crypto-dl
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_xslt ext_gcrypt ext_gnutls
)
