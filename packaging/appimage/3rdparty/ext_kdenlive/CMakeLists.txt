ExternalProject_Add(
  ext_kdenlive
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY http://anongit.kde.org/kdenlive.git
  GIT_TAG refactoring_timeline

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE} 
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DBUILD_TESTING=FALSE
    -DBUILD_TESTS=FALSE

  DEPENDS ext_mlt
)
