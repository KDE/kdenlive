add_subdirectory(dialogs)
add_subdirectory(jobs)
set(kdenlive_SRCS
  ${kdenlive_SRCS}
  project/clipmanager.cpp
#  project/clipproperties.cpp
#  project/clippropertiesmanager.cpp
  project/clipstabilize.cpp
  project/cliptranscode.cpp
#  project/folderprojectitem.cpp
  project/invaliddialog.cpp
  project/projectcommands.cpp
  project/projectmanager.cpp
#  project/projectitem.cpp
#  project/projectlist.cpp
#  project/projectlistview.cpp
#  project/subprojectitem.cpp
  project/effectsettings.cpp
  project/transitionsettings.cpp
  project/notesplugin.cpp
  PARENT_SCOPE)
