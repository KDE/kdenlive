SET(PREFIX_ext_openssl "${EXT_INSTALL_DIR}" )

ExternalProject_Add(
  ext_openssl
        DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
        URL https://www.openssl.org/source/openssl-1.1.1j.tar.gz
        URL_HASH SHA256=aaf2fcb575cdf6491b98ab4829abf78a3dec8402b8b81efc8f23c00d443981bf

        CONFIGURE_COMMAND <SOURCE_DIR>/config --prefix=${PREFIX_ext_openssl} --openssldir=openssl

        BUILD_COMMAND make depend COMMAND make
        INSTALL_COMMAND make install

        INSTALL_DIR ${PREFIX_ext_openssl}
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${PREFIX_ext_openssl} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} ${GLOBAL_PROFILE}

        UPDATE_COMMAND ""
)
