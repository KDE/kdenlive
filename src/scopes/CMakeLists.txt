add_subdirectory(colorscopes)
add_subdirectory(audioscopes)
set(kdenlive_SRCS
  ${kdenlive_SRCS}
  scopes/scopemanager.cpp
  scopes/abstractscopewidget.cpp
  PARENT_SCOPE)

