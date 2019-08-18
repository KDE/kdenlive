# The OpenCV contrib repo external project
ExternalProject_Add(
  ext_opencv_contrib
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://github.com/opencv/opencv_contrib/archive/4.1.0.tar.gz
  URL_MD5 5357a038f54ba15c771abb0d91a5519d

  # This is a support repository for OpenCV 4.x and does not contain any
  # build or install rules. This will be hooked into OpenCV which will control
  # those steps.
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

# The OpenCV external project
ExternalProject_Add(
  ext_opencv
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://github.com/opencv/opencv/archive/4.1.0.tar.gz
  URL_MD5 b80c59c7e4feee6a00608315e02b0b73
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE} -DOPENCV_EXTRA_MODULES_PATH=${EXT_BUILD_DIR}/deps-build/ext_opencv/ext_opencv_contrib-prefix/src/ext_opencv_contrib/modules -DOPENCV_GENERATE_PKGCONFIG=ON -DBUILD_opencv_freetype=OFF  -DCPU_BASELINE=SSE4_2 -DCPU_DISPATCH=SSE4_2
  DEPENDS ext_opencv_contrib
)
