# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

add_subdirectory(audiolevels)

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  audiomixer/mixerwidget.cpp
  audiomixer/mixermanager.cpp
  audiomixer/audioslider.cpp
  audiomixer/mixerseparator.cpp
  PARENT_SCOPE)


