
add_subdirectory( widgets )

find_package(Nepomuk REQUIRED)

include_directories(
    ${CMAKE_SOURCE_DIR}/src/widgets
    ${LIBMLT_INCLUDE_DIR}
    ${LIBMLTPLUS_INCLUDE_DIR}
    ${LIBFFMPEG_INCLUDE_DIR}
    ${NEPOMUK_INCLUDES}
)

LINK_DIRECTORIES(
${LIBMLT_LIBRARY}
${LIBMLTPLUS_LIBRARY}
${LIBFFMPEG_LIBRARY}
${NEPOMUK_LIBRARIES}
)


kde4_add_ui_files(kdenlive_UI
  widgets/timeline_ui.ui
  widgets/monitor_ui.ui
  widgets/colorclip_ui.ui
  widgets/configmisc_ui.ui
  widgets/configenv_ui.ui
  widgets/effectlist_ui.ui
  widgets/effectstack_ui.ui
  widgets/profiledialog_ui.ui
)
 
set(kdenlive_SRCS 
  addclipcommand.cpp
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
  projectlistview.cpp
  trackview.cpp
  docclipbase.cpp
  timecode.cpp
  monitormanager.cpp
  documenttrack.cpp
  documentvideotrack.cpp
  documentaudiotrack.cpp
  headertrack.cpp
  trackpanelfunctionfactory.cpp
  trackpanelfunction.cpp
  trackpanelclipmovefunction.cpp
  customtrackview.cpp
  clipitem.cpp
  labelitem.cpp
  moveclipcommand.cpp
  resizeclipcommand.cpp
  addtimelineclipcommand.cpp
  kthumb.cpp
  clipmanager.cpp
  effectslist.cpp
  initeffects.cpp
  effectslistview.cpp
  addeffectcommand.cpp
  effectstackview.cpp
  parameterplotter.cpp

)

kde4_add_kcfg_files(kdenlive_SRCS GENERATE_MOC kdenlivesettings.kcfgc )

kde4_add_executable(kdenlive ${kdenlive_SRCS} ${kdenlive_UI})

target_link_libraries(kdenlive 
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS} 
  ${LIBMLTPLUS_LIBRARY}
  ${LIBMLT_LIBRARY}
  ${NEPOMUK_LIBRARIES}
)
 
install(TARGETS kdenlive DESTINATION ${BIN_INSTALL_DIR})
install( FILES kdenliveui.rc DESTINATION  ${DATA_INSTALL_DIR}/kdenlive )
install (FILES kdenlivesettings.kcfg DESTINATION share/config.kcfg)
install (FILES kdenlive.desktop DESTINATION share/applications/kde)

