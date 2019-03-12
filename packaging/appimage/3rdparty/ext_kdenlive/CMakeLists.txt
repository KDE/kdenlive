ExternalProject_Add(
  ext_kdenlive
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY http://anongit.kde.org/kdenlive.git
  GIT_TAG refactoring_timeline

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_mlt
)
