ExternalProject_Add(
  ext_ffmpeg
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://anonscm.debian.org/git/pkg-multimedia/ffmpeg.git
  GIT_TAG debian/7%3.3.4-2
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} --extra-ldflags="-L${EXT_INSTALL_DIR}/lib -lstdc++ -lm -lrt -ldl" --extra-cflags="-I$EXT_INSTALL_DIR/include" --enable-shared --enable-avfilter --enable-gpl --enable-avresample --enable-libx265 --enable-libvpx --enable-libx264 --disable-doc
  BUILD_ALWAYS TRUE
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
