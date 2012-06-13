add_subdirectory(commands)

set(project_SRCS
  project/abstractclipplugin.cpp
  project/abstractprojectclip.cpp
  project/abstractprojectitem.cpp
  project/abstracttimelineclip.cpp
  project/clippluginmanager.cpp
  project/producerwrapper.cpp
  project/project.cpp
  project/projectfolder.cpp
  project/projectmanager.cpp
  project/timeline.cpp
  project/timelinetrack.cpp

  ${commands_SRCS}
  PARENT_SCOPE
)
