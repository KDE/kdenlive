set(QT_USE_QTDBUS 1)
set(QT_USE_QTOPENGL 1)
set(QT_USE_QTSCRIPT 1)
set(QT_USE_QTSVG 1)
set(QT_USE_QTXML 1)

find_package(OpenGL REQUIRED)
if(APPLE)
  find_package(SDL REQUIRED)
endif(APPLE)
macro_optional_find_package(Nepomuk)
macro_optional_find_package(QJSON)

option(WITH_V4L "Build capture support with Video4Linux" ON)
option(WITH_JogShuttle "Build Jog/Shuttle support" ON)



if(WITH_V4L)
  # This can be changed to support FreeBSD as soon as we move to a newer V4L2
  # header.
  find_package(LibV4L2)
  macro_log_feature(LIBV4L2_FOUND "libv4l" "Collection of video4linux support libraries" "http://hansdegoede.livejournal.com, http://linuxtv.org/hg/~hgoede/libv4l/" FALSE "" "Required for better webcam support")
  if(LIBV4L2_FOUND)
    set(BUILD_V4L TRUE)
  endif(LIBV4L2_FOUND)
endif(WITH_V4L)

if(WITH_JogShuttle)
  check_include_files(linux/input.h HAVE_LINUX_INPUT_H)
  if(HAVE_LINUX_INPUT_H)
    set(BUILD_JogShuttle TRUE)
  endif(HAVE_LINUX_INPUT_H)
endif(WITH_JogShuttle)

macro_log_feature(QT_QTOPENGL_FOUND
  "QtOpenGL"
  "Qt bindings for the OpenGL library"
  "http://qt.nokia.com/products/qt"
  TRUE
  ${QT_MIN_VERSION}
  "QtOpenGL library and headers (most likely called libqt4-opengl and libqt4-opengl-dev)"
)

macro_log_feature(QT_QTSCRIPT_FOUND
  "QtScript"
  "Qt module providing core scripting facilities"
  "http://qt.nokia.com/products/qt"
  TRUE
  ${QT_MIN_VERSION}
  "QtScript library (most likely called libqt4-script)"
)

if(APPLE)
  macro_log_feature(SDL_FOUND
    "SDL"
    "Cross-platform multimedia library"
    "http://www.libsdl.org"
    TRUE
  )
endif(APPLE)

macro_log_feature(Nepomuk_FOUND
  "Nepomuk"
  "Support for the Nepomuk semantic desktop system"
  "http://nepomuk.semanticdesktop.org"
)

macro_log_feature(QJSON_FOUND
  "QJson"
  "Qt-based library that maps JSON data to QVariant objects"
  "http://qjson.sourceforge.net"
  FALSE
  ""
  "Required to build the rotoscoping filter and for Freesound.org queries"
)


add_subdirectory(beziercurve)
add_subdirectory(colorcorrection)
add_subdirectory(commands)
add_subdirectory(lib)
add_subdirectory(projecttree)
add_subdirectory(utils)
add_subdirectory(databackup)
add_subdirectory(effectstack)
add_subdirectory(mimetypes)
add_subdirectory(onmonitoritems)
add_subdirectory(scopes)
add_subdirectory(simplekeyframes)
add_subdirectory(stopmotion)

if(QJSON_FOUND)
  add_subdirectory(onmonitoritems/rotoscoping)
  add_subdirectory(rotoscoping)
endif(QJSON_FOUND)

if(BUILD_V4L)
  add_subdirectory(v4l)
endif(BUILD_V4L)

list(APPEND kdenlive_SRCS
  main.cpp
  mainwindow.cpp

  KoSliderCombo.cpp
  abstractclipitem.cpp
  abstractgroupitem.cpp
  abstractmonitor.cpp
  archivewidget.cpp
  choosecolorwidget.cpp
  clipdurationdialog.cpp
  clipitem.cpp
  clipmanager.cpp
  clipproperties.cpp
  cliptranscode.cpp
  clipstabilize.cpp
  colorpickerwidget.cpp
  colorplaneexport.cpp
  colortools.cpp
  complexparameter.cpp
  cornerswidget.cpp
  customruler.cpp
  customtrackscene.cpp
  customtrackview.cpp
  definitions.cpp
  docclipbase.cpp
  documentchecker.cpp
  documentvalidator.cpp
  doubleparameterwidget.cpp
  dragvalue.cpp
  dvdwizard.cpp
  dvdwizardchapters.cpp
  dvdwizardmenu.cpp
  dvdwizardvob.cpp
  effectslist.cpp
  effectslistview.cpp
  effectslistwidget.cpp
  effectstackedit.cpp
  encodingprofilesdialog.cpp
  folderprojectitem.cpp
  gentime.cpp
  geometryval.cpp
  geometrywidget.cpp
  graphicsscenerectmove.cpp
  guide.cpp
  headertrack.cpp
  initeffects.cpp
  kdenlivedoc.cpp
  kdenlivesettingsdialog.cpp
  keyframeedit.cpp
  keyframehelper.cpp
  kis_cubic_curve.cpp
  kis_curve_widget.cpp
  kthumb.cpp
  managecapturesdialog.cpp
  markerdialog.cpp
  mltdevicecapture.cpp
  monitor.cpp
  monitoreditwidget.cpp
  monitormanager.cpp
  monitorscene.cpp
  noteswidget.cpp
  parameterplotter.cpp
  positionedit.cpp
  profilesdialog.cpp
  projectitem.cpp
  projectlist.cpp
  projectlistview.cpp
  projectsettings.cpp
  recmonitor.cpp
  renderer.cpp
  renderwidget.cpp
  slideshowclip.cpp
  smallruler.cpp
  spacerdialog.cpp
  statusbarmessagelabel.cpp
  subprojectitem.cpp
  timecode.cpp
  timecodedisplay.cpp
  titledocument.cpp
  titlewidget.cpp
  trackdialog.cpp
  tracksconfigdialog.cpp
  trackview.cpp
  transition.cpp
  transitionsettings.cpp
  unicodedialog.cpp
  wizard.cpp
)

kde4_add_ui_files(kdenlive_UIS
  widgets/addtrack_ui.ui
  widgets/archivewidget_ui.ui
  widgets/audiospectrum_ui.ui
  widgets/backupdialog_ui.ui
  widgets/bezierspline_ui.ui
  widgets/boolval_ui.ui
  widgets/clipdurationdialog_ui.ui
  widgets/clipproperties_ui.ui
  widgets/cliptranscode_ui.ui
  widgets/collapsiblewidget_ui.ui
  widgets/clipstabilize_ui.ui
  widgets/colorclip_ui.ui
  widgets/colorplaneexport_ui.ui
  widgets/configcapture_ui.ui
  widgets/configenv_ui.ui
  widgets/configjogshuttle_ui.ui
  widgets/configmisc_ui.ui
  widgets/configproject_ui.ui
  widgets/configsdl_ui.ui
  widgets/configtimeline_ui.ui
  widgets/configtranscode_ui.ui
  widgets/dvdwizardchapters_ui.ui
  widgets/dvdwizardmenu_ui.ui
  widgets/dvdwizardstatus_ui.ui
  widgets/dvdwizardvob_ui.ui
  widgets/effectlist_ui.ui
  widgets/effectstack_ui.ui
  widgets/effectstack2_ui.ui
  widgets/freesound_ui.ui
  widgets/geometryval_ui.ui
  widgets/geometrywidget_ui.ui
  widgets/histogram_ui.ui
  widgets/keyframedialog_ui.ui
  widgets/keyframeeditor_ui.ui
  widgets/keyframewidget_ui.ui
  widgets/listval_ui.ui
  widgets/managecaptures_ui.ui
  widgets/manageencodingprofile_ui.ui
  widgets/markerdialog_ui.ui
  widgets/missingclips_ui.ui
  widgets/monitoreditwidget_ui.ui
  widgets/profiledialog_ui.ui
  widgets/projectsettings_ui.ui
  widgets/recmonitor_ui.ui
  widgets/renderwidget_ui.ui
  widgets/rgbparade_ui.ui
  widgets/saveprofile_ui.ui
  widgets/slideshowclip_ui.ui
  widgets/smconfig_ui.ui
  widgets/spacerdialog_ui.ui
  widgets/spectrogram_ui.ui
  widgets/stopmotion_ui.ui
  widgets/templateclip_ui.ui
  widgets/timeline_ui.ui
  widgets/timelinebuttons_ui.ui
  widgets/titlewidget_ui.ui
  widgets/trackheader_ui.ui
  widgets/tracksconfigdialog_ui.ui
  widgets/transitionsettings_ui.ui
  widgets/unicodedialog_ui.ui
  widgets/urlval_ui.ui
  widgets/vectorscope_ui.ui
  widgets/waveform_ui.ui
  widgets/wipeval_ui.ui
  widgets/wizardcapture_ui.ui
  widgets/wizardcheck_ui.ui
  widgets/wizardextra_ui.ui
  widgets/wizardmltcheck_ui.ui
  widgets/wizardstandard_ui.ui
  widgets/keywordval_ui.ui
  widgets/fontval_ui.ui
  widgets/cutjobdialog_ui.ui
  widgets/scenecutdialog_ui.ui
  widgets/importkeyframesdialog_ui.ui
)

if(OPENGL_FOUND)
  list(APPEND kdenlive_SRCS videoglwidget.cpp)
endif(OPENGL_FOUND)

if(BUILD_JogShuttle)
  list(APPEND kdenlive_SRCS
    jogaction.cpp
    jogshuttle.cpp
    jogshuttleconfig.cpp
  )
endif(BUILD_JogShuttle)

kde4_add_kcfg_files(kdenlive_SRCS kdenlivesettings.kcfgc)
qt4_add_dbus_adaptor(kdenlive_SRCS
  org.kdenlive.MainWindow.xml
  mainwindow.h
  MainWindow
)
qt4_add_resources(kdenlive_SRCS widgets/resources.qrc)
kde4_add_executable(kdenlive
  ${kdenlive_SRCS}
  ${kdenlive_UIS}
)

# To compile kiss_fft.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")

# KDE definitions and include directories *must* always come first, Qt follows
# (to avoid breaking builds when KDE and/or Qt are installed to different
# prefixes).
add_definitions(${KDE4_DEFINITIONS})

include_directories(
  ${CMAKE_BINARY_DIR}
  ${KDE4_INCLUDES} # Adds Qt include directories too.
  ${LIBMLT_INCLUDE_DIR}
  ${LIBMLTPLUS_INCLUDE_DIR}
)


# Adds Qt definitions and include directories, and sets QT_LIBRARIES according
# to the components requested in find_package().
include(${QT_USE_FILE})

if(${KDE_VERSION} VERSION_LESS 4.3.80)
  set(KDE4_KNEWSTUFF_LIBS ${KDE4_KNEWSTUFF2_LIBS})
else(${KDE_VERSION} VERSION_LESS 4.3.80)
  set(KDE4_KNEWSTUFF_LIBS ${KDE4_KNEWSTUFF3_LIBS})
endif(${KDE_VERSION} VERSION_LESS 4.3.80)

target_link_libraries(kdenlive
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${KDE4_KNEWSTUFF_LIBS}
  ${KDE4_KNOTIFYCONFIG_LIBRARY}
  ${KDE4_KROSSUI_LIBS}
  ${KDE4_SOLID_LIBS}
  ${QT_LIBRARIES}
  ${LIBMLT_LIBRARY}
  ${LIBMLTPLUS_LIBRARY}
  ${CMAKE_DL_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)

if(Q_WS_X11)
  include_directories(${X11_Xlib_INCLUDE_PATH})
  target_link_libraries(kdenlive ${X11_LIBRARIES})
endif(Q_WS_X11)

if(OPENGL_FOUND AND QT_QTOPENGL_FOUND)
  add_definitions(-DUSE_OPENGL)
  target_link_libraries(kdenlive ${OPENGL_LIBRARIES})
endif(OPENGL_FOUND AND QT_QTOPENGL_FOUND)

if(SDL_FOUND)
  target_link_libraries(kdenlive ${SDL_LIBRARY})
endif(SDL_FOUND)

if(Nepomuk_FOUND)
  add_definitions(-DUSE_NEPOMUK)
  include_directories(${NEPOMUK_INCLUDES})
  target_link_libraries(kdenlive ${NEPOMUK_LIBRARIES})
endif(Nepomuk_FOUND)

if(QJSON_FOUND)
  add_definitions(-DUSE_QJSON)
  include_directories(${QJSON_INCLUDE_DIR})
  target_link_libraries(kdenlive ${QJSON_LIBRARIES})
endif(QJSON_FOUND)

if(BUILD_V4L)
  include_directories(${LIBV4L2_INCLUDE_DIR})
  target_link_libraries(kdenlive ${LIBV4L2_LIBRARY})  
  add_definitions(-DUSE_V4L)
endif(BUILD_V4L)

if(BUILD_JogShuttle)
  add_definitions(-DUSE_JOGSHUTTLE)
endif(BUILD_JogShuttle)

install(TARGETS kdenlive DESTINATION ${BIN_INSTALL_DIR})
install(FILES
  kdenliveui.rc
  kdenlive.notifyrc
  DESTINATION ${DATA_INSTALL_DIR}/kdenlive
)
install(FILES kdenlivesettings.kcfg DESTINATION ${KCFG_INSTALL_DIR})
install(FILES
  kdenlive_projectprofiles.knsrc
  kdenlive_renderprofiles.knsrc
  kdenlive_titles.knsrc
  kdenlive_wipes.knsrc
  kdenlivetranscodingrc
  DESTINATION ${CONFIG_INSTALL_DIR}
)
kde4_install_icons(${ICON_INSTALL_DIR})
install(FILES kdenlive.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
