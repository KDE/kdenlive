set(ofx_openSPIncludeDir "${EXT_INSTALL_DIR}/include/OpenSP")
set(ofx_openSPLibDir "${EXT_INSTALL_DIR}/lib")

ExternalProject_Add(
  ext_ofx
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://downloads.sourceforge.net/project/libofx/libofx/libofx-0.9.13.tar.gz
  URL_MD5 dde6276ffe2999f86bed5bf2cc80f7b2

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} ${AT_SHARED_ONLY} --with-opensp-includes=${ofx_openSPIncludeDir} --with-opensp-libs=${ofx_openSPLibDir}
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_opensp
)
