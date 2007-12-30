
add_subdirectory( widgets )


include_directories(
    ${CMAKE_SOURCE_DIR}/src/widgets
    ${LIBMLT_INCLUDE_DIR}
    ${LIBMLTPLUS_INCLUDE_DIR}
    ${LIBFFMPEG_INCLUDE_DIR}
)

LINK_DIRECTORIES(
${LIBMLT_LIBRARY}
${LIBMLTPLUS_LIBRARY}
${LIBFFMPEG_LIBRARY}
)


kde4_add_ui_files(kdenlive_UI
  widgets/timeline_ui.ui
  widgets/monitor_ui.ui
  widgets/colorclip_ui.ui
  widgets/configmisc_ui.ui
)
 
set(kdenlive_SRCS 
  main.cpp
  mainwindow.cpp
  customruler.cpp
  smallruler.cpp
  projectlist.cpp
  projectitem.cpp
  monitor.cpp
  gentime.cpp
  renderer.cpp
  kdenlivedoc.cpp
  trackview.cpp
  docclipbase.cpp
  timecode.cpp
  monitormanager.cpp
)

kde4_add_kcfg_files(kdenlive_SRCS GENERATE_MOC kdenlivesettings.kcfgc )

kde4_add_executable(kdenlive ${kdenlive_SRCS} ${kdenlive_UI})

target_link_libraries(kdenlive 
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS} 
  ${LIBMLTPLUS_LIBRARY}
  ${LIBMLT_LIBRARY}
)
 
install(TARGETS kdenlive DESTINATION ${BIN_INSTALL_DIR})
install( FILES kdenliveui.rc DESTINATION  ${DATA_INSTALL_DIR}/kdenlive )
install (FILES kdenlive.kcfg DESTINATION share/config.kcfg)