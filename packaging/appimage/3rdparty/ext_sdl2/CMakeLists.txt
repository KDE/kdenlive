ExternalProject_Add(
  ext_sdl2
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://libsdl.org/release/SDL2-2.0.9.tar.gz
  URL_MD5 f2ecfba915c54f7200f504d8b48a5dfe

  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} ${AT_HOST} --with-alsa-prefix=${EXT_INSTALL_DIR}/usr/lib --with-alsa-inc-prefix=${EXT_INSTALL_DIR}/usr/include/
  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install

  DEPENDS ext_alsa
)
