set(QT_USE_QTOPENGL 1)

add_subdirectory(bin)
add_subdirectory(effectsystem)
add_subdirectory(producersystem)
add_subdirectory(monitor)
add_subdirectory(project)
add_subdirectory(timelineview)
add_subdirectory(widgets)

set(kdenlivecore_LIB_SRCS
  core.cpp
  fraction.cpp
  mainwindow.cpp
  pluginmanager.cpp
  timecode.cpp
  timecodeformatter.cpp
  undoview.cpp

  ${bin_SRCS}
  ${effectsystem_SRCS}
  ${producersystem_SRCS}
  ${monitor_SRCS}
  ${project_SRCS}
  ${timelineview_SRCS}
  ${widget_SRCS}
)

kde4_add_ui_files(kdenlivecore_UIS
  ${effectsystem_UIS}
  ${producersystem_UIS}
  ${timelineview_UIS}
)

message(${kdenlivecore_UIS})
message(${CMAKE_CURRENT_BINARY_DIR})
message(${CMAKE_BINARY_DIR})

# KDE definitions and include directories *must* always come first, Qt follows
# (to avoid breaking builds when KDE and/or Qt are installed to different
# prefixes).
add_definitions(${KDE4_DEFINITIONS})

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${KDE4_INCLUDES}
  ${LIBMLT_INCLUDE_DIR}
  ${LIBMLTPLUS_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)

# Adds Qt definitions and include directories, and sets QT_LIBRARIES according
# to the components requested in find_package().
include(${QT_USE_FILE})

kde4_add_kcfg_files(kdenlivecore_LIB_SRCS kdenlivesettings.kcfgc)

kde4_add_library(kdenlivecore SHARED ${kdenlivecore_LIB_SRCS} ${kdenlivecore_UIS})

target_link_libraries(kdenlivecore
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${QT_LIBRARIES}
  ${LIBMLT_LIBRARY}
  ${LIBMLTPLUS_LIBRARY}
  ${OPENGL_LIBRARIES}
  ${GLEW_LIBRARIES}
)

if(Q_WS_X11)
  target_link_libraries(kdenlivecore ${X11_LIBRARIES})
endif(Q_WS_X11)

set_target_properties(kdenlivecore PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install(TARGETS kdenlivecore ${INSTALL_TARGETS_DEFAULT_ARGS} )
install(FILES kdenlivesettings.kcfg DESTINATION ${KCFG_INSTALL_DIR})
install(FILES kdenliveui.rc DESTINATION ${DATA_INSTALL_DIR}/kdenlive)
