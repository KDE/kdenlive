set(QT_USE_QTDBUS 1)
set(QT_USE_QTOPENGL 1)
set(QT_USE_QTSCRIPT 1)
set(QT_USE_QTSVG 1)
set(QT_USE_QTXML 1)

find_package(OpenGL REQUIRED)

find_package(GLEW REQUIRED)

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

macro_log_feature(GLEW_FOUND
  "GLEW"
  "OpenGL Extension Wrangler Library (GLEW)"
  ""
  TRUE
)

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

add_subdirectory(core)
add_subdirectory(plugins)

list(APPEND kdenlive_SRCS
  main.cpp
#   mainwindow.cpp
# 
#   KoSliderCombo.cpp
#   abstractclipitem.cpp
#   abstractgroupitem.cpp
#   abstractmonitor.cpp
#   archivewidget.cpp
#   choosecolorwidget.cpp
#   clipdurationdialog.cpp
#   clipitem.cpp
#   clipmanager.cpp
#   clipproperties.cpp
#   cliptranscode.cpp
#   clipstabilize.cpp
#   colorplaneexport.cpp
#   colortools.cpp
#   complexparameter.cpp
#   cornerswidget.cpp
#   customruler.cpp
#   customtrackscene.cpp
#   customtrackview.cpp
#   definitions.cpp
#   docclipbase.cpp
#   documentchecker.cpp
#   documentvalidator.cpp
#   doubleparameterwidget.cpp
#   dvdwizard.cpp
#   dvdwizardchapters.cpp
#   dvdwizardmenu.cpp
#   dvdwizardvob.cpp
#   effectslist.cpp
#   effectslistview.cpp
#   effectslistwidget.cpp
#   effectstackedit.cpp
#   effectstackview.cpp
#   encodingprofilesdialog.cpp
#   folderprojectitem.cpp
#   gentime.cpp
#   geometryval.cpp
#   geometrywidget.cpp
#   graphicsscenerectmove.cpp
#   guide.cpp
#   headertrack.cpp
#   initeffects.cpp
#   kdenlivedoc.cpp
#   kdenlivesettingsdialog.cpp
#   keyframeedit.cpp
#   keyframehelper.cpp
#   kis_cubic_curve.cpp
#   kis_curve_widget.cpp
#   kthumb.cpp
#   managecapturesdialog.cpp
#   markerdialog.cpp
#   mltdevicecapture.cpp
#   monitor.cpp
#   monitoreditwidget.cpp
#   monitormanager.cpp
#   monitorscene.cpp
#   noteswidget.cpp
#   parameterplotter.cpp
#   positionedit.cpp
#   profilesdialog.cpp
#   projectitem.cpp
#   projectlist.cpp
#   projectlistview.cpp
#   projectsettings.cpp
#   recmonitor.cpp
#   renderer.cpp
#   renderwidget.cpp
#   slideshowclip.cpp
#   smallruler.cpp
#   spacerdialog.cpp
#   statusbarmessagelabel.cpp
#   subprojectitem.cpp
#   timecode.cpp
#   timecodedisplay.cpp
#   titledocument.cpp
#   titlewidget.cpp
#   trackdialog.cpp
#   tracksconfigdialog.cpp
#   trackview.cpp
#   transition.cpp
#   transitionsettings.cpp
#   unicodedialog.cpp
#   wizard.cpp
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

# KDE definitions and include directories *must* always come first, Qt follows
# (to avoid breaking builds when KDE and/or Qt are installed to different
# prefixes).
add_definitions(${KDE4_DEFINITIONS})

include_directories(
  ${CMAKE_SOURCE_DIR}/src/core
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${KDE4_INCLUDES} # Adds Qt include directories too.
  ${LIBMLT_INCLUDE_DIR}
  ${LIBMLTPLUS_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
)


# Adds Qt definitions and include directories, and sets QT_LIBRARIES according
# to the components requested in find_package().
include(${QT_USE_FILE})


# qt4_add_dbus_adaptor(kdenlive_SRCS
#   org.kdenlive.MainWindow.xml
#   mainwindow.h
#   MainWindow
# )
kde4_add_executable(kdenlive
  ${kdenlive_SRCS}
)

# To compile kiss_fft.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")

if(${KDE_VERSION} VERSION_LESS 4.3.80)
  set(KDE4_KNEWSTUFF_LIBS ${KDE4_KNEWSTUFF2_LIBS})
else(${KDE_VERSION} VERSION_LESS 4.3.80)
  set(KDE4_KNEWSTUFF_LIBS ${KDE4_KNEWSTUFF3_LIBS})
endif(${KDE_VERSION} VERSION_LESS 4.3.80)

target_link_libraries(kdenlive
  kdenlivecore
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
# install(FILES
#   kdenliveui.rc
#   kdenlive.notifyrc
#   DESTINATION ${DATA_INSTALL_DIR}/kdenlive
# )

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
