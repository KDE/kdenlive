ExternalProject_Add(
  ext_bigshot
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  GIT_REPOSITORY https://bitbucket.org/leo_sutic/bigsh0t.git
  GIT_TAG 9533c9caa6b583f8c5f2cba50c4713eec057c371
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}

  INSTALL_COMMAND ""
  COMMAND ${CMAKE_COMMAND} -E echo FILE ${BUILD_DIR}/bigsh0t_eq_mask.so  >> ${EXT_INSTALL_DIR}/lib/frei0r-1/bigsh0t_eq_mask.so
  COMMAND ${CMAKE_COMMAND} -E echo FILE ${BUILD_DIR}/bigsh0t_eq_to_rect.so  >> ${EXT_INSTALL_DIR}/lib/frei0r-1/bigsh0t_eq_to_rect.so
  COMMAND ${CMAKE_COMMAND} -E echo FILE ${BUILD_DIR}/bigsh0t_rect_to_eq.so  >> ${EXT_INSTALL_DIR}/lib/frei0r-1/bigsh0t_rect_to_eq.so
  COMMAND ${CMAKE_COMMAND} -E echo FILE ${BUILD_DIR}/bigsh0t_stabilize_360.so  >> ${EXT_INSTALL_DIR}/lib/frei0r-1/bigsh0t_stabilize_360.so
  COMMAND ${CMAKE_COMMAND} -E echo FILE ${BUILD_DIR}/bigsh0t_hemi_to_eq.so  >> ${EXT_INSTALL_DIR}/lib/frei0r-1/bigsh0t_hemi_to_eq.so
  COMMAND ${CMAKE_COMMAND} -E echo FILE ${BUILD_DIR}/bigsh0t_transform_360.so  >> ${EXT_INSTALL_DIR}/lib/frei0r-1/bigsh0t_transform_360.so

  #INSTALL_COMMAND cp ${BUILD_DIR}/{bigsh0t_eq_mask.so,bigsh0t_rect_to_eq.so,bigsh0t_eq_to_rect.so,bigsh0
  #t_stabilize_360.so,bigsh0t_hemi_to_eq.so,bigsh0t_transform_360.so} ${EXT_INSTALL_DIR}/lib/frei0r-1/
)
