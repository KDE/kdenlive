# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>, Vincent PINON <vincent.pinon@laposte.net>

add_subdirectory(scopes)

if (QT_MAJOR_VERSION STREQUAL "6")

if (APPLE)
  set(kdenlive_SRCS
    ${kdenlive_SRCS}
    monitor/videowidget.cpp
    monitor/metalvideowidget.mm
    monitor/abstractmonitor.cpp
    monitor/monitor.cpp
    monitor/monitormanager.cpp
    monitor/recmanager.cpp
    monitor/qmlmanager.cpp
    monitor/monitorproxy.cpp
  PARENT_SCOPE)
elseif (WIN32)
  set(kdenlive_SRCS
    ${kdenlive_SRCS}
    monitor/videowidget.cpp
    monitor/openglvideowidget.cpp
    monitor/d3dvideowidget.cpp
    monitor/abstractmonitor.cpp
    monitor/monitor.cpp
    monitor/monitormanager.cpp
    monitor/recmanager.cpp
    monitor/qmlmanager.cpp
    monitor/monitorproxy.cpp
    PARENT_SCOPE)
else()
  set(kdenlive_SRCS
    ${kdenlive_SRCS}
    monitor/videowidget.cpp
    monitor/openglvideowidget.cpp
    monitor/abstractmonitor.cpp
    monitor/monitor.cpp
    monitor/monitormanager.cpp
    monitor/recmanager.cpp
    monitor/qmlmanager.cpp
    monitor/monitorproxy.cpp
    PARENT_SCOPE)
endif()

else()
  set(kdenlive_SRCS
  ${kdenlive_SRCS}
  monitor/glwidget.cpp
  monitor/abstractmonitor.cpp
  monitor/monitor.cpp
  monitor/monitormanager.cpp
  monitor/recmanager.cpp
  monitor/qmlmanager.cpp
  monitor/monitorproxy.cpp
  PARENT_SCOPE)
endif()


