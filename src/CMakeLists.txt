add_subdirectory( widgets )
add_subdirectory( mimetypes )

macro_optional_find_package(Nepomuk)
pkg_check_modules(qimageblitz qimageblitz)

if (Nepomuk_FOUND)
    INCLUDE_DIRECTORIES(NEPOMUK_INCLUDES )
    set( optional_libs ${NEPOMUK_LIBRARIES} )
    add_definitions(-DNEPOMUK)
endif (Nepomuk_FOUND)

if (qimageblitz_FOUND)
    INCLUDE_DIRECTORIES(QIMAGEBLITZ_INCLUDES )
    set( optional_libs ${QIMAGEBLITZ_LIBRARIES} )
    add_definitions(-DQIMAGEBLITZ)
    MESSAGE( STATUS "Found QImageBlitz: ${QIMAGEBLITZ_LIBRARIES}" )
else (qimageblitz_FOUND)
    MESSAGE( WARNING "Install QImageBlitz devel package for improved stopmotion features" )
endif (qimageblitz_FOUND)

if (Q_WS_X11)
    include_directories (${X11_Xlib_INCLUDE_PATH} ) 
endif (Q_WS_X11)

if (APPLE)
  find_package(OpenGL)
  set(QT_USE_QTOPENGL TRUE)
else (APPLE)
  macro_optional_find_package(OpenGL)
endif (APPLE)

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
    ${CMAKE_SOURCE_DIR}/src/colorcorrection
    ${CMAKE_SOURCE_DIR}/src/onmonitoritems
)

LINK_LIBRARIES(
${LIBMLT_LIBRARY}
${LIBMLTPLUS_LIBRARY}
${optional_libs}
)



kde4_add_ui_files(kdenlive_UI
  widgets/timeline_ui.ui
  widgets/recmonitor_ui.ui
  widgets/colorclip_ui.ui
  widgets/slideshowclip_ui.ui
  widgets/configmisc_ui.ui
  widgets/configenv_ui.ui
  widgets/configtimeline_ui.ui
  widgets/configsdl_ui.ui
  widgets/configcapture_ui.ui
  widgets/configtranscode_ui.ui
  widgets/configproject_ui.ui
  widgets/effectlist_ui.ui
  widgets/effectstack_ui.ui
  widgets/profiledialog_ui.ui
  widgets/projectsettings_ui.ui
  widgets/keyframewidget_ui.ui
  widgets/listval_ui.ui
  widgets/wipeval_ui.ui
  widgets/boolval_ui.ui
  widgets/titlewidget_ui.ui
  widgets/unicodedialog_ui.ui
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
  widgets/dvdwizardstatus_ui.ui
  widgets/dvdwizardchapters_ui.ui
  widgets/missingclips_ui.ui
  widgets/cliptranscode_ui.ui
  widgets/templateclip_ui.ui
  widgets/keyframeeditor_ui.ui
  widgets/timecodedisplay_ui.ui
  widgets/tracksconfigdialog_ui.ui
  widgets/urlval_ui.ui
  widgets/vectorscope_ui.ui
  widgets/colorplaneexport_ui.ui
  widgets/waveform_ui.ui
  widgets/rgbparade_ui.ui
  widgets/histogram_ui.ui
  widgets/geometrywidget_ui.ui
  widgets/stopmotion_ui.ui
  widgets/monitorscenecontrolwidget_ui.ui
  widgets/cornerswidget_ui.ui
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
  unicodedialog.cpp
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
  documentvalidator.cpp
  cliptranscode.cpp
  keyframeedit.cpp
  positionedit.cpp
  trackdialog.cpp
  subprojectitem.cpp
  folderprojectitem.cpp
  addclipcutcommand.cpp
  editclipcutcommand.cpp
  KoSliderCombo.cpp
  kis_cubic_curve.cpp
  kis_curve_widget.cpp
  timecodedisplay.cpp
  tracksconfigdialog.cpp
  configtrackscommand.cpp
  abstractscopewidget.cpp
  rebuildgroupcommand.cpp
  histogram.cpp
  rgbparade.cpp
  vectorscope.cpp
  waveform.cpp
  colorplaneexport.cpp
  colortools.cpp
  colorcorrection/histogramgenerator.cpp
  colorcorrection/rgbparadegenerator.cpp
  colorcorrection/vectorscopegenerator.cpp
  colorcorrection/waveformgenerator.cpp
  razorgroupcommand.cpp
  colorpickerwidget.cpp
  choosecolorwidget.cpp
  monitorscene.cpp
  geometrywidget.cpp
  doubleparameterwidget.cpp
  audiosignal.cpp
  blackmagic/include/DeckLinkAPIDispatch.cpp
  stopmotion/stopmotion.cpp
  stopmotion/capturehandler.cpp
  blackmagic/capture.cpp
  blackmagic/devices.cpp
  v4l/v4lcapture.cpp
  v4l/src.c
  v4l/src_v4l2.c
  onmonitoritems/abstractonmonitoritem.cpp 
  onmonitoritems/onmonitorrectitem.cpp
  onmonitoritems/onmonitorcornersitem.cpp
  monitorscenecontrolwidget.cpp
  cornerswidget.cpp
)


add_subdirectory( ${CMAKE_SOURCE_DIR}/src/colorcorrection )
add_subdirectory( ${CMAKE_SOURCE_DIR}/src/onmonitoritems )

add_definitions( ${KDE4_DEFINITIONS} )

if(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD" OR NO_JOGSHUTTLE)
  add_definitions(-DNO_JOGSHUTTLE)
else(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD" OR NO_JOGSHUTTLE)
  set(kdenlive_SRCS jogshuttle.cpp ${kdenlive_SRCS})
endif(APPLE OR CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD" OR NO_JOGSHUTTLE)

if (APPLE OR OPENGL_FOUND)
  add_definitions(-DUSE_OPEN_GL)
  set(kdenlive_SRCS videoglwidget.cpp ${kdenlive_SRCS})
endif (APPLE OR OPENGL_FOUND)


# Add libdl
find_library( DL_LIB NAMES ${CMAKE_DL_LIBS} )
MESSAGE( STATUS "using dl library: ${DL_LIB}" )

kde4_add_kcfg_files(kdenlive_SRCS kdenlivesettings.kcfgc )
QT4_ADD_DBUS_ADAPTOR(kdenlive_SRCS org.kdenlive.MainWindow.xml mainwindow.h MainWindow)
kde4_add_executable(kdenlive ${kdenlive_SRCS} ${kdenlive_UI})

if (${KDE_VERSION} VERSION_LESS 4.3.80)
  set(KNEWSTUFF_LIBS ${KDE4_KNEWSTUFF2_LIBS})
else (${KDE_VERSION} VERSION_LESS 4.3.80)
  set(KNEWSTUFF_LIBS ${KDE4_KNEWSTUFF3_LIBS})
endif (${KDE_VERSION} VERSION_LESS 4.3.80)

target_link_libraries(kdenlive
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${KNEWSTUFF_LIBS}
  ${LIBMLTPLUS_LIBRARY}
  ${LIBMLT_LIBRARY}
  ${optional_libs}
  ${KDE4_KNOTIFYCONFIG_LIBRARY}
  ${KDE4_KROSSUI_LIBS}
  ${CMAKE_DL_LIBS}
)

if (Q_WS_X11)
  target_link_libraries(kdenlive ${X11_LIBRARIES})
endif (Q_WS_X11)

if (APPLE)
  find_package(SDL REQUIRED)
  target_link_libraries(kdenlive ${SDL_LIBRARY})
  target_link_libraries(kdenlive ${QT_QTOPENGL_LIBRARY})
  target_link_libraries(kdenlive ${OPENGL_LIBRARIES})
else (APPLE)
  add_definitions(-DHAVE_V4L2)
  if (OPENGL_FOUND)
      target_link_libraries(kdenlive ${QT_QTOPENGL_LIBRARY})
      target_link_libraries(kdenlive ${OPENGL_LIBRARIES})
  endif (OPENGL_FOUND)
endif (APPLE)

install(TARGETS kdenlive DESTINATION ${BIN_INSTALL_DIR})
install( FILES kdenliveui.rc kdenlive.notifyrc DESTINATION  ${DATA_INSTALL_DIR}/kdenlive )
install (FILES kdenlivesettings.kcfg DESTINATION ${KCFG_INSTALL_DIR})
install (FILES kdenlive.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
kde4_install_icons(${ICON_INSTALL_DIR})
install( FILES kdenlive_titles.knsrc kdenlive_wipes.knsrc kdenlive_renderprofiles.knsrc kdenlive_projectprofiles.knsrc kdenlivetranscodingrc DESTINATION  ${CONFIG_INSTALL_DIR} )
kde4_install_icons( ${ICON_INSTALL_DIR} )


