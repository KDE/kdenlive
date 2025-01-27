# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>

set(kdenlive_SRCS
  ${kdenlive_SRCS}
  jobs/abstracttask.cpp
  jobs/taskmanager.cpp
  jobs/audiolevels/audiolevelstask.cpp
  jobs/audiolevels/generators.cpp
  jobs/cliploadtask.cpp
  jobs/proxytask.cpp
  jobs/stabilizetask.cpp
  jobs/speedtask.cpp
  jobs/transcodetask.cpp
  jobs/filtertask.cpp
  jobs/masktask.cpp
  jobs/melttask.cpp
  jobs/cachetask.cpp
  jobs/scenesplittask.cpp
  jobs/cuttask.cpp
  jobs/customjobtask.cpp
  PARENT_SCOPE)
