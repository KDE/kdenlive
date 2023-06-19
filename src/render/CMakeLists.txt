if(NODBUS)
    list(APPEND kdenlive_SRCS
      render/renderserver.cpp)
endif()

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  render/renderrequest.cpp
  PARENT_SCOPE)
