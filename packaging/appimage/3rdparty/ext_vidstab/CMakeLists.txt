ExternalProject_Add(
  ext_vidstab
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://github.com/georgmartius/vid.stab.git

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}

)
