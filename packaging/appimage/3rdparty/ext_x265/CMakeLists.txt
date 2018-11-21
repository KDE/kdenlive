ExternalProject_Add(
  ext_x265
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://anonscm.debian.org/git/pkg-multimedia/x265.git

  BUILD_IN_SOURCE 1
  UPDATE_DISCONNECTED 1

  CONFIGURE_COMMAND cd build/linux && PATH=$ENV{PATH} cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=$ENV{DEPS_INSTALL_PREFIX} -DENABLE_PIC:BOOL=ON -D ENABLE_SHARED:BOOL=ON ../../source
  BUILD_COMMAND cd build/linux && PATH=$ENV{PATH} make
  INSTALL_COMMAND cd build/linux && PATH=$ENV{PATH} make install
)
