# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>, Nicolas Carion <french.ebook.lover@gmail.com>

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  bin/abstractprojectitem.cpp
  bin/bin.cpp
  bin/bincommands.cpp
  bin/binplaylist.cpp
  bin/clipcreator.cpp
  bin/filewatcher.cpp
  bin/mediabrowser.cpp
  bin/generators/generators.cpp
  bin/model/markerlistmodel.cpp
  bin/model/markersortmodel.cpp
  bin/model/subtitlemodel.cpp
  bin/projectclip.cpp
  bin/projectfolder.cpp
  bin/projectitemmodel.cpp
  bin/projectsortproxymodel.cpp
  bin/playlistsubclip.cpp
  bin/projectsubclip.cpp
  bin/sequenceclip.cpp
  bin/playlistclip.cpp
  bin/tagwidget.cpp
  PARENT_SCOPE
)
