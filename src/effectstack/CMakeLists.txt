add_subdirectory(widgets/curves)
set(kdenlive_SRCS
  ${kdenlive_SRCS}
  effectstack/abstractcollapsiblewidget.cpp
  effectstack/collapsibleeffect.cpp
  effectstack/collapsiblegroup.cpp
  effectstack/effectstackedit.cpp
  effectstack/effectstackview2.cpp
  effectstack/graphicsscenerectmove.cpp
  effectstack/keyframehelper.cpp
  effectstack/animkeyframeruler.cpp
  PARENT_SCOPE)

