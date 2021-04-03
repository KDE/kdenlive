ExternalProject_Add(
  ext_vidstab
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://github.com/georgmartius/vid.stab.git
  GIT_TAG f6b686dc03628aa596515c799107595745e46ae6

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}

)
