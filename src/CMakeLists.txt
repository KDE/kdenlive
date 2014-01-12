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

find_package(NepomukCore QUIET)
set_package_properties(NepomukCore PROPERTIES DESCRIPTION "The Nepomuk Core libraries" URL "http://www.kde.org" TYPE RECOMMENDED PURPOSE "Support for the Nepomuk semantic desktop system")


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
add_subdirectory(widgets)

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
  clipdurationdialog.cpp
  clipitem.cpp
  clipmanager.cpp
  clipproperties.cpp
  cliptranscode.cpp
  clipstabilize.cpp
  colorplaneexport.cpp
  colortools.cpp
  complexparameter.cpp
  customruler.cpp
  customtrackscene.cpp
  customtrackview.cpp
  definitions.cpp
  docclipbase.cpp
  documentchecker.cpp
  documentvalidator.cpp
  dragvalue.cpp
  dvdwizard.cpp
  dvdwizardchapters.cpp
  dvdwizardmenu.cpp
  dvdwizardvob.cpp
  effectslist.cpp
  effectslistview.cpp
  effectstackedit.cpp
  encodingprofilesdialog.cpp
  folderprojectitem.cpp
  gentime.cpp
  geometryval.cpp
  graphicsscenerectmove.cpp
  guide.cpp
  headertrack.cpp
  initeffects.cpp
  kdenlivedoc.cpp
  kdenlivesettingsdialog.cpp
  keyframeedit.cpp
  keyframehelper.cpp
  kis_cubic_curve.cpp
  kthumb.cpp
  managecapturesdialog.cpp
  markerdialog.cpp
  mltdevicecapture.cpp
  monitor.cpp
  monitormanager.cpp
  monitorscene.cpp
  parameterplotter.cpp
  positionedit.cpp
  profilesdialog.cpp
  projectitem.cpp
  projectlist.cpp
  projectlistview.cpp
  projectsettings.cpp
  recmonitor.cpp
  renderer.cpp
  slideshowclip.cpp
  smallruler.cpp
  spacerdialog.cpp
  statusbarmessagelabel.cpp
  subprojectitem.cpp
  timecode.cpp
  timecodedisplay.cpp
  titledocument.cpp
  trackdialog.cpp
  tracksconfigdialog.cpp
  trackview.cpp
  transition.cpp
  transitionsettings.cpp
  unicodedialog.cpp
  wizard.cpp
)

kde4_add_ui_files(kdenlive_UIS
  ui/addtrack_ui.ui
  ui/archivewidget_ui.ui
  ui/audiospectrum_ui.ui
  ui/backupdialog_ui.ui
  ui/bezierspline_ui.ui
  ui/boolval_ui.ui
  ui/clipdurationdialog_ui.ui
  ui/clipproperties_ui.ui
  ui/cliptranscode_ui.ui
  ui/collapsiblewidget_ui.ui
  ui/clipstabilize_ui.ui
  ui/colorclip_ui.ui
  ui/colorplaneexport_ui.ui
  ui/configcapture_ui.ui
  ui/configenv_ui.ui
  ui/configjogshuttle_ui.ui
  ui/configmisc_ui.ui
  ui/configproject_ui.ui
  ui/configsdl_ui.ui
  ui/configtimeline_ui.ui
  ui/configtranscode_ui.ui
  ui/dvdwizardchapters_ui.ui
  ui/dvdwizardmenu_ui.ui
  ui/dvdwizardstatus_ui.ui
  ui/dvdwizardvob_ui.ui
  ui/effectlist_ui.ui
  ui/effectstack_ui.ui
  ui/effectstack2_ui.ui
  ui/freesound_ui.ui
  ui/geometryval_ui.ui
  ui/geometrywidget_ui.ui
  ui/histogram_ui.ui
  ui/keyframedialog_ui.ui
  ui/keyframeeditor_ui.ui
  ui/keyframewidget_ui.ui
  ui/listval_ui.ui
  ui/managecaptures_ui.ui
  ui/manageencodingprofile_ui.ui
  ui/markerdialog_ui.ui
  ui/missingclips_ui.ui
  ui/monitoreditwidget_ui.ui
  ui/profiledialog_ui.ui
  ui/projectsettings_ui.ui
  ui/recmonitor_ui.ui
  ui/renderwidget_ui.ui
  ui/rgbparade_ui.ui
  ui/saveprofile_ui.ui
  ui/slideshowclip_ui.ui
  ui/smconfig_ui.ui
  ui/spacerdialog_ui.ui
  ui/spectrogram_ui.ui
  ui/stopmotion_ui.ui
  ui/templateclip_ui.ui
  ui/timeline_ui.ui
  ui/timelinebuttons_ui.ui
  ui/titlewidget_ui.ui
  ui/trackheader_ui.ui
  ui/tracksconfigdialog_ui.ui
  ui/transitionsettings_ui.ui
  ui/unicodewidget_ui.ui
  ui/urlval_ui.ui
  ui/vectorscope_ui.ui
  ui/waveform_ui.ui
  ui/wipeval_ui.ui
  ui/wizardcapture_ui.ui
  ui/wizardcheck_ui.ui
  ui/wizardextra_ui.ui
  ui/wizardmltcheck_ui.ui
  ui/wizardstandard_ui.ui
  ui/keywordval_ui.ui
  ui/fontval_ui.ui
  ui/cutjobdialog_ui.ui
  ui/scenecutdialog_ui.ui
  ui/importkeyframesdialog_ui.ui
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
qt4_add_resources(kdenlive_SRCS ui/resources.qrc)
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

if(NepomukCore_FOUND)
    add_definitions(-DUSE_NEPOMUKCORE)
    include_directories(${NEPOMUK_CORE_INCLUDE_DIR})
    target_link_libraries(kdenlive ${NEPOMUK_CORE_LIBRARY})
else(NepomukWidgets_FOUND)
  if(Nepomuk_FOUND)
    add_definitions(-DUSE_NEPOMUK)
    include_directories(${NEPOMUK_INCLUDES})
    target_link_libraries(kdenlive ${NEPOMUK_LIBRARIES})
  endif(Nepomuk_FOUND)
endif()

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
add_subdirectory(icons)
install(FILES kdenlive.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
