set(kdenlive_SRCS
  ${kdenlive_SRCS}
  effects/abstracteffectlist.cpp
  effects/abstracteffectrepositoryitem.cpp
  effects/effect.cpp
  effects/effectdescription.cpp
  effects/effectdevice.cpp
  effects/effectrepository.cpp
  PARENT_SCOPE
)


add_subdirectory(parameter)
