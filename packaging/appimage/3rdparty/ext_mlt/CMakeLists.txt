ExternalProject_Add(
  ext_mlt
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://github.com/mltframework/mlt.git
  GIT_TAG master

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE} -DMOD_OPENCV=ON -DMOD_MOVIT=ON
  BUILD_IN_SOURCE 1
)
