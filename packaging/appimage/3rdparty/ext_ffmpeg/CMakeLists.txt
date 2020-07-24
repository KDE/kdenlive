ExternalProject_Add(                                                                                                                                                       
  ext_ffmpeg                                                                                                                                                               
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}                                                                                                                                         
  GIT_REPOSITORY https://anonscm.debian.org/git/pkg-multimedia/ffmpeg.git                                                                                                  
# A stupid crlf issue prevents changing branch using normal pipeline, so                                                                                                   
# we have a crazy workaround to delete offending file and checkout the wanted branch                                                                                       
  UPDATE_COMMAND git checkout debian/7%4.3-3                                                                                                                               
#  PATCH_COMMAND rm <SOURCE_DIR>/tests/ref/fate/sub-scc                                                                                                                    
  GIT_TAG debian/master
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=${EXT_INSTALL_DIR} --extra-ldflags="-L${EXT_INSTALL_DIR}/lib -lstdc++ -lm -lrt -ldl" --extra-cflags="-I$EXT_INSTALL_DIR/include" --enable-shared --enable-avfilter --enable-gpl --enable-avresample --enable-libx265 --enable-libvpx --enable-libx264 --disable-doc --enable-libmp3lame --enable-libopus --enable-libvorbis
  BUILD_ALWAYS TRUE
  BUILD_IN_SOURCE 1
  BUILD_COMMAND $(MAKE) clean
  COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
)
