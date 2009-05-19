
add_subdirectory( widgets )
add_subdirectory( mimetypes )

find_package(Nepomuk REQUIRED)

include_directories (
    ${KDE4_INCLUDE_DIR}
    ${KDE4_INCLUDE_DIR}/KDE
    ${QT_INCLUDES}
    ${LIBMLT_INCLUDE_DIR}
    ${LIBMLTPLUS_INCLUDE_DIR}
    ${QDBUS_INCLUDE_DIRS} 
    ${CMAKE_SOURCE_DIR} 
    ${CMAKE_BINARY_DIR} 

    ${CMAKE_SOURCE_DIR}/src/widgets
    ${NEPOMUK_INCLUDES}
)

LINK_LIBRARIES(
${LIBMLT_LIBRARY}
${LIBMLTPLUS_LIBRARY}
${NEPOMUK_LIBRARIES}
)


kde4_add_ui_files(kdenlive_UI
  widgets/timeline_ui.ui
  widgets/monitor_ui.ui
  widgets/recmonitor_ui.ui
  widgets/colorclip_ui.ui
  widgets/slideshowclip_ui.ui
  widgets/configmisc_ui.ui
  widgets/configenv_ui.ui
  widgets/configdisplay_ui.ui
  widgets/configsdl_ui.ui
  widgets/configcapture_ui.ui
  widgets/configtranscode_ui.ui
  widgets/effectlist_ui.ui
  widgets/effectstack_ui.ui
  widgets/profiledialog_ui.ui
  widgets/projectsettings_ui.ui
  widgets/keyframewidget_ui.ui
  widgets/constval_ui.ui
  widgets/listval_ui.ui
  widgets/wipeval_ui.ui
  widgets/boolval_ui.ui
  widgets/colorval_ui.ui
  widgets/positionval_ui.ui
  widgets/titlewidget_ui.ui
  widgets/timelinebuttons_ui.ui
  widgets/renderwidget_ui.ui
  widgets/saveprofile_ui.ui
  widgets/transitionsettings_ui.ui
  widgets/configjogshuttle_ui.ui
  widgets/trackheader_ui.ui
  widgets/clipproperties_ui.ui
  widgets/markerdialog_ui.ui
  widgets/keyframedialog_ui.ui
  widgets/clipdurationdialog_ui.ui
  widgets/managecaptures_ui.ui
  widgets/geometryval_ui.ui
  widgets/wizardstandard_ui.ui
  widgets/wizardextra_ui.ui
  widgets/wizardcheck_ui.ui
  widgets/wizardmltcheck_ui.ui
  widgets/spacerdialog_ui.ui
  widgets/addtrack_ui.ui
  widgets/dvdwizardvob_ui.ui
  widgets/dvdwizardmenu_ui.ui
  widgets/dvdwizardiso_ui.ui
  widgets/dvdwizardstatus_ui.ui
  widgets/dvdwizardchapters_ui.ui
  widgets/missingclips_ui.ui
  widgets/cliptranscode_ui.ui
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
  headertrack.cpp
  customtrackview.cpp
  clipitem.cpp
  moveclipcommand.cpp
  resizeclipcommand.cpp
  razorclipcommand.cpp
  addtimelineclipcommand.cpp
  addmarkercommand.cpp
  kthumb.cpp
  clipmanager.cpp
  effectslist.cpp
  initeffects.cpp
  effectslistview.cpp
  addeffectcommand.cpp
  editeffectcommand.cpp
  moveeffectcommand.cpp
  effectstackview.cpp
  effectstackedit.cpp
  parameterplotter.cpp
  profilesdialog.cpp
  projectsettings.cpp
  kdenlivesettingsdialog.cpp
  complexparameter.cpp
  titlewidget.cpp
  graphicsscenerectmove.cpp
  effectslistwidget.cpp
  titledocument.cpp
  transition.cpp
  renderwidget.cpp
  abstractclipitem.cpp
  transitionsettings.cpp
  recmonitor.cpp
  addtransitioncommand.cpp
  edittransitioncommand.cpp
  addfoldercommand.cpp
  editfoldercommand.cpp
  clipproperties.cpp
  movetransitioncommand.cpp
  slideshowclip.cpp
  markerdialog.cpp
  guide.cpp
  editguidecommand.cpp
  statusbarmessagelabel.cpp
  regiongrabber.cpp
  editkeyframecommand.cpp
  clipdurationdialog.cpp
  managecapturesdialog.cpp
  changespeedcommand.cpp
  geometryval.cpp
  wizard.cpp
  customtrackscene.cpp
  abstractgroupitem.cpp
  keyframehelper.cpp
  editclipcommand.cpp
  insertspacecommand.cpp
  spacerdialog.cpp
  addtrackcommand.cpp
  changetrackcommand.cpp
  movegroupcommand.cpp
  dvdwizardvob.cpp
  dvdwizardmenu.cpp
  dvdwizard.cpp
  locktrackcommand.cpp
  groupclipscommand.cpp
  splitaudiocommand.cpp
  changecliptypecommand.cpp
  documentchecker.cpp
  dvdwizardchapters.cpp
  documentconvert.cpp
  cliptranscode.cpp
)

add_definitions( ${KDE4_DEFINITIONS} )

if(NO_JOGSHUTTLE)
  add_definitions(-DNO_JOGSHUTTLE)
else(NO_JOGSHUTTLE)
  set(kdenlive_SRCS jogshuttle.cpp ${kdenlive_SRCS})
endif(NO_JOGSHUTTLE)

kde4_add_kcfg_files(kdenlive_SRCS kdenlivesettings.kcfgc )
QT4_ADD_DBUS_ADAPTOR(kdenlive_SRCS org.kdenlive.MainWindow.xml mainwindow.h MainWindow)
kde4_add_executable(kdenlive ${kdenlive_SRCS} ${kdenlive_UI})


target_link_libraries(kdenlive 
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${KDE4_KNEWSTUFF2_LIBS}
  ${LIBMLTPLUS_LIBRARY}
  ${LIBMLT_LIBRARY}
  ${NEPOMUK_LIBRARIES}
  ${KDE4_KNOTIFYCONFIG_LIBRARY}
  ${KDE4_KROSSUI_LIBS}
)

if (APPLE)
  find_package(SDL REQUIRED)
  target_link_libraries(kdenlive ${SDL_LIBRARY})
endif (APPLE)

install(TARGETS kdenlive DESTINATION ${BIN_INSTALL_DIR})
install( FILES kdenliveui.rc kdenlive.notifyrc DESTINATION  ${DATA_INSTALL_DIR}/kdenlive )
install (FILES kdenlivesettings.kcfg DESTINATION ${KCFG_INSTALL_DIR})
install (FILES kdenlive.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install (FILES application-x-kdenlive.svgz video-mlt-playlist.svgz DESTINATION ${ICON_INSTALL_DIR}/oxygen/scalable/mimetypes)
install( FILES kdenlive.knsrc kdenlive_render.knsrc kdenlive_mltprofiles.knsrc DESTINATION  ${CONFIG_INSTALL_DIR} )
kde4_install_icons( ${ICON_INSTALL_DIR} )


