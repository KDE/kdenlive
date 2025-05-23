# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Vincent Pinon <vpinon@kde.org>, Jean-Baptiste Mardelle <jb@kdenlive.org>

set(QT_DONT_USE_QTGUI 1)
if(USE_DBUS)
    set(QT_USE_QTDBUS 1)
endif()
include_directories(
    ${MLT_INCLUDE_DIR}
    ${MLTPP_INCLUDE_DIR}
)

set(kdenlive_render_SRCS
  kdenlive_render.cpp
  renderjob.cpp
  ../src/lib/localeHandling.cpp
)

ecm_qt_declare_logging_category(kdenlive_render_SRCS
    HEADER kdenlive_renderer_debug.h
    IDENTIFIER KDENLIVE_RENDERER_LOG
    CATEGORY_NAME org.kde.kdenlive.render
    DESCRIPTION "kdenlive"
    EXPORT KDENLIVE
)

add_executable(kdenlive_render ${kdenlive_render_SRCS})
ecm_mark_nongui_executable(kdenlive_render)

target_link_libraries(kdenlive_render
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Xml
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES})
if(NOT USE_DBUS)
    target_compile_definitions(kdenlive_render PRIVATE NODBUS)
    target_link_libraries(kdenlive_render Qt${QT_MAJOR_VERSION}::Network)
else()
    target_link_libraries(kdenlive_render Qt${QT_MAJOR_VERSION}::DBus)
    target_link_libraries(kdenlive_render Qt${QT_MAJOR_VERSION}::Network)
endif()

install(TARGETS kdenlive_render DESTINATION ${KDE_INSTALL_BINDIR})
