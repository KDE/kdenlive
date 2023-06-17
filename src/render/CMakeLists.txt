set(kdenlive_SRCS
  ${kdenlive_SRCS}
  render/renderrequest.cpp
  PARENT_SCOPE)

if(NODBUS)
    set(kdenlive_SRCS
      ${kdenlive_SRCS}
      render/renderserver.cpp
      PARENT_SCOPE)
endif()
