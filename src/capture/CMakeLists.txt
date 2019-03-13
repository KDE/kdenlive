if(LIBV4L2_FOUND)
    list(APPEND kdenlive_SRCS
    capture/v4lcapture.cpp
    )
endif()

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  capture/managecapturesdialog.cpp
  capture/mltdevicecapture.cpp
  capture/mediacapture.cpp
  PARENT_SCOPE)


