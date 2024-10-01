# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Jean-Baptiste Mardelle <jb@kdenlive.org>, Vincent PINON <vincent.pinon@laposte.net>

add_subdirectory(scopes)

if (APPLE)
    list(APPEND kdenlive_SRCS
        monitor/videowidget.cpp
        monitor/metalvideowidget.mm
    )
elseif (WIN32)
    list(APPEND kdenlive_SRCS
        monitor/videowidget.cpp
        monitor/openglvideowidget.cpp
        monitor/d3dvideowidget.cpp
    )
else()
    list(APPEND kdenlive_SRCS
        monitor/videowidget.cpp
        monitor/openglvideowidget.cpp
    )
endif()

set(kdenlive_SRCS
    ${kdenlive_SRCS}
    monitor/abstractmonitor.cpp
    monitor/monitor.cpp
    monitor/monitormanager.cpp
    monitor/recmanager.cpp
    monitor/qmlmanager.cpp
    monitor/monitorproxy.cpp
    PARENT_SCOPE
)
