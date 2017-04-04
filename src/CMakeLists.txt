add_definitions(${Qt5Gui_DEFINITIONS})
if(${Qt5Gui_OPENGL_IMPLEMENTATION} STREQUAL "GL")
    find_package(OpenGL REQUIRED)
    set_package_properties(OpenGL PROPERTIES
        DESCRIPTION "the OpenGL library"
        URL ""
        TYPE RUNTIME
        PURPOSE "")
else()
    find_package(OpenGLES REQUIRED)
    set_package_properties(OpenGLES PROPERTIES
        DESCRIPTION "the OpenGLES library"
        URL ""
        TYPE RUNTIME
        PURPOSE "")
endif()

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -pedantic -Wextra")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wcast-qual -Wcast-align -Wfloat-equal -Wpointer-arith")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wunreachable-code -Wchar-subscripts -Wcomment -Wformat")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror-implicit-function-declaration -Wmain -Wmissing-braces")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wparentheses -Wsequence-point -Wreturn-type -Wswitch")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized -Wreorder -Wundef -Wshadow -Wwrite-strings")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-compare  -Wconversion")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmissing-noreturn  -Wsign-conversion -Wunused ")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wstrict-aliasing -Wstrict-overflow -Wconversion")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdisabled-optimization")
if (CMAKE_COMPILER_IS_GNUCXX)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wlogical-op -Wunsafe-loop-optimizations ")
endif()
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wunused-parameter -Wshadow -Wno-variadic-macros -Wno-float-conversion")

find_package(PkgConfig QUIET)
execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=mltdatadir mlt-framework
    OUTPUT_VARIABLE MLT_DATADIR
    RESULT_VARIABLE MLT_DATADIR_failed)
if (NOT MLT_DATADIR_failed)
    string(REGEX REPLACE "[\r\n]" "" MLT_DATADIR "${MLT_DATADIR}")
endif()

execute_process(
    COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=meltbin mlt-framework
    OUTPUT_VARIABLE MLT_MELTBIN
    RESULT_VARIABLE MLT_MELTBIN_failed)
if (NOT MLT_MELTBIN_failed)
    string(REGEX REPLACE "[\r\n]" "" MLT_MELTBIN "${MLT_MELTBIN}")
endif()

configure_file( mlt_config.h.in ${CMAKE_BINARY_DIR}/generated/mlt_config.h )
include_directories( ${CMAKE_BINARY_DIR}/generated/ ) # Make sure it can be included...

option(WITH_JogShuttle "Build Jog/Shuttle support" ON)

set(FFMPEG_SUFFIX "" CACHE STRING "FFmpeg custom suffix")
find_package(LibV4L2)
set_package_properties(LibV4L2 PROPERTIES 
    DESCRIPTION "Collection of video4linux support libraries"
    URL "http://freecode.com/projects/libv4l"
    TYPE RUNTIME
    PURPOSE "Required for better webcam support")

if(WITH_JogShuttle)
    check_include_files(linux/input.h HAVE_LINUX_INPUT_H)
    if(HAVE_LINUX_INPUT_H)
        set(BUILD_JogShuttle TRUE)
    endif(HAVE_LINUX_INPUT_H)
endif()

set_package_properties(OpenGL PROPERTIES
    DESCRIPTION "the OpenGL library"
    URL ""
    TYPE RUNTIME
    PURPOSE "")

set_package_properties(QtScript PROPERTIES
    DESCRIPTION   "Qt module providing core scripting facilities"
    URL "http://qt.nokia.com/products/qt"
    TYPE RUNTIME
    PURPOSE "")

#if(APPLE)
#  macro_log_feature(SDL_FOUND
#    "SDL"
#    "Cross-platform multimedia library"
#    "http://www.libsdl.org"
#    TRUE
#  )
#endif(APPLE)
#add_definitions( -DQT_NO_CAST_FROM_ASCII )
#add_definitions( -DQT_NO_CAST_TO_ASCII )

install(FILES kdenlivesettings.kcfg DESTINATION ${KCFG_INSTALL_DIR})
kconfig_add_kcfg_files(kdenlive_SRCS kdenlivesettings.kcfgc)

add_subdirectory(doc)
add_subdirectory(project)
add_subdirectory(timeline)
add_subdirectory(effectstack)
add_subdirectory(dialogs)
add_subdirectory(effectslist)
add_subdirectory(scopes)
add_subdirectory(capture)
add_subdirectory(dvdwizard)
add_subdirectory(lib)
add_subdirectory(monitor)
add_subdirectory(onmonitoritems)
add_subdirectory(simplekeyframes)
add_subdirectory(stopmotion)
add_subdirectory(titler)
add_subdirectory(utils)
add_subdirectory(onmonitoritems/rotoscoping)
add_subdirectory(mltcontroller)
add_subdirectory(bin)
add_subdirectory(qml)
add_subdirectory(profiles)

if (Qt5WebKitWidgets_FOUND)
    add_subdirectory(qt-oauth-lib)
endif()

add_subdirectory(library)

list(APPEND kdenlive_SRCS
    colortools.cpp
    definitions.cpp
    gentime.cpp
    doc/kthumb.cpp
    main.cpp
    mainwindow.cpp
    renderer.cpp
    statusbarmessagelabel.cpp
    timecode.cpp
    timecodedisplay.cpp
    layoutmanagement.cpp
    hidetitlebars.cpp
    mltconnection.cpp
    core.cpp
    )

ecm_qt_declare_logging_category(kdenlive_SRCS HEADER kdenlive_debug.h IDENTIFIER KDENLIVE_LOG CATEGORY_NAME org.kde.multimedia.kdenlive)

ki18n_wrap_ui(kdenlive_UIS
    ui/addtrack_ui.ui
    ui/archivewidget_ui.ui
    ui/audiospectrum_ui.ui
    ui/backupdialog_ui.ui
    ui/bezierspline_ui.ui
    ui/boolparamwidget_ui.ui
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
    ui/freesound_ui.ui
    ui/geometrywidget_ui.ui
    ui/histogram_ui.ui
    ui/keyframedialog_ui.ui
    ui/keyframeeditor_ui.ui
    ui/keyframewidget_ui.ui
    ui/listparamwidget_ui.ui
    ui/logindialog_ui.ui
    ui/managecaptures_ui.ui
    ui/manageencodingprofile_ui.ui
    ui/markerdialog_ui.ui
    ui/missingclips_ui.ui
    ui/monitoreditwidget_ui.ui
    ui/profiledialog_ui.ui
    ui/projectsettings_ui.ui
    ui/qtextclip_ui.ui
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
    ui/gradientedit_ui.ui
    ui/selectivecolor_ui.ui
    ui/clipspeed_ui.ui
    )

if(BUILD_JogShuttle)
    list(APPEND kdenlive_SRCS
        jogshuttle/jogmanager.cpp
        jogshuttle/jogaction.cpp
        jogshuttle/jogshuttle.cpp
        jogshuttle/jogshuttleconfig.cpp
        )
endif()

# Sets the icon on Windows and OSX
file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../data/icons/*-apps-kdenlive.png")
ecm_add_app_icon(kdenlive_SRCS ICONS ${ICONS_SRCS})

qt5_add_dbus_adaptor(kdenlive_SRCS
    org.kdenlive.MainWindow.xml
    mainwindow.h
    MainWindow
    )
qt5_add_resources(kdenlive_SRCS icons.qrc ui/resources.qrc uiresources.qrc)
add_executable(kdenlive
    ${kdenlive_SRCS}
    ${kdenlive_UIS}
    )

# To compile kiss_fft.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")

# KDE definitions and include directories *must* always come first, Qt follows
# (to avoid breaking builds when KDE and/or Qt are installed to different
# prefixes).

include_directories(
    ${CMAKE_BINARY_DIR}
    ${MLT_INCLUDE_DIR}
    ${MLTPP_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/external
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    )


# Adds Qt definitions and include directories, and sets QT_LIBRARIES according
# to the components requested in find_package().
#include(${QT_USE_FILE})

target_link_libraries(kdenlive
    KF5::WidgetsAddons
    KF5::Archive
    KF5::CoreAddons
    KF5::KIOCore
    KF5::KIOFileWidgets
    KF5::KIOWidgets
    KF5::NotifyConfig
    KF5::NewStuff
    KF5::DBusAddons
    KF5::XmlGui
    KF5::GuiAddons
    KF5::Notifications
    KF5::TextWidgets
    KF5::IconThemes
    KF5::Crash
    Qt5::Svg
    ${OPENGL_LIBRARIES}
    ${OPENGLES_LIBRARIES}
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    kiss_fft
    )

message(STATUS "Found MLT++: ${MLTPP_LIBRARIES}")

if (KF5_FILEMETADATA)
    add_definitions(-DKF5_USE_FILEMETADATA)
    target_link_libraries(kdenlive KF5::FileMetaData)
endif()

qt5_use_modules( kdenlive Script Widgets Concurrent Qml Quick)

if (Qt5WebKitWidgets_FOUND)
    message(STATUS "Found Qt5 WebKitWidgets. You can use your Freesound.org credentials to download files")
    add_definitions(-DQT5_USE_WEBKIT)
    target_link_libraries(kdenlive Qt5::WebKitWidgets)
else()
    message(STATUS "Qt5 WebKitWidgets not found. You cannot use your Freesound.org credentials, only preview files can be downloaded from the Online Resources Widget")
endif()


if(Q_WS_X11)
    include_directories(${X11_Xlib_INCLUDE_PATH})
    target_link_libraries(kdenlive ${X11_LIBRARIES})
endif(Q_WS_X11)

if(SDL_FOUND)
    target_link_libraries(kdenlive ${SDL_LIBRARY})
endif(SDL_FOUND)

if(LIBV4L2_FOUND)
    include_directories(${LIBV4L2_INCLUDE_DIR})
    target_link_libraries(kdenlive ${LIBV4L2_LIBRARY})
    add_definitions(-DUSE_V4L)
endif()

if(BUILD_JogShuttle)
    add_definitions(-DUSE_JOGSHUTTLE)
    target_link_libraries(kdenlive
        media_ctrl
        )
endif()

install(TARGETS kdenlive DESTINATION ${BIN_INSTALL_DIR})
install(FILES kdenliveui.rc DESTINATION ${KXMLGUI_INSTALL_DIR}/kdenlive)

