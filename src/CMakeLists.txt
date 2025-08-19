# SPDX-FileCopyrightText: 2007 Kdenlive contributors
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL


### Find packages files
find_package(LibV4L2)
set_package_properties(LibV4L2 PROPERTIES
    DESCRIPTION "video4linux support libraries"
    URL "https://linuxtv.org/downloads/v4l-utils/"
    TYPE RUNTIME
    PURPOSE "Required for better webcam support")

if(WIN32)
    set(MLT_MELTBIN "melt.exe")
    set(MLT_DATADIR "../share/mlt")
elseif(APPLE)
    set(MLT_MELTBIN "melt")
    set(MLT_DATADIR "../Resources/mlt")
else()
    find_package(PkgConfig QUIET)
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=mltdatadir mlt-framework-7
        OUTPUT_VARIABLE MLT_DATADIR
        RESULT_VARIABLE MLT_DATADIR_failed)
    if(NOT MLT_DATADIR_failed)
        string(REGEX REPLACE "[\r\n]" "" MLT_DATADIR "${MLT_DATADIR}")
    endif()

    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=meltbin mlt-framework-7
        OUTPUT_VARIABLE MLT_MELTBIN
        RESULT_VARIABLE MLT_MELTBIN_failed)
    if(NOT MLT_MELTBIN_failed)
        string(REGEX REPLACE "[\r\n]" "" MLT_MELTBIN "${MLT_MELTBIN}")
    endif()
endif()
configure_file(mlt_config.h.in ${CMAKE_BINARY_DIR}/mlt_config.h)

check_include_files(linux/input.h HAVE_LINUX_INPUT_H)
if(HAVE_LINUX_INPUT_H)
    list(APPEND kdenlive_SRCS
        jogshuttle/jogmanager.cpp
        jogshuttle/jogaction.cpp
        jogshuttle/jogshuttle.cpp
        jogshuttle/jogshuttleconfig.cpp)
endif()

### Find source files
add_subdirectory(abstractmodel)
add_subdirectory(assets)
add_subdirectory(audiomixer)
add_subdirectory(bin)
add_subdirectory(capture)
add_subdirectory(dialogs)
add_subdirectory(doc)
add_subdirectory(effects)
add_subdirectory(jobs)
add_subdirectory(lib)
add_subdirectory(library)
add_subdirectory(mltcontroller)
add_subdirectory(monitor)
add_subdirectory(onlineresources)
add_subdirectory(otio)
add_subdirectory(profiles)
add_subdirectory(project)
add_subdirectory(pythoninterfaces)
add_subdirectory(render)
add_subdirectory(renderpresets)
add_subdirectory(scopes)
add_subdirectory(timeline2)
add_subdirectory(titler)
add_subdirectory(transitions)
add_subdirectory(utils)
add_subdirectory(widgets)
add_subdirectory(xml)
add_subdirectory(layouts)

## toplevel source code files
list(APPEND kdenlive_SRCS
    core.cpp
    definitions.cpp
    docktitlebarmanager.cpp
    mainwindow.cpp
    mltconnection.cpp
    statusbarmessagelabel.cpp
    undohelper.cpp
    glaxnimatelauncher.cpp
    filefilter.cpp
    powermanagementinterface.cpp
)

if(CRASH_AUTO_TEST)
    list(APPEND kdenlive_SRCS logger.cpp)
endif()

ecm_qt_declare_logging_category(kdenlive_SRCS
    HEADER kdenlive_debug.h
    IDENTIFIER KDENLIVE_LOG
    CATEGORY_NAME org.kde.kdenlive
    OLD_CATEGORY_NAMES org.kde.multimedia.kdenlive
    DESCRIPTION "Kdenlive"
    EXPORT KDENLIVE
)

if(USE_DBUS)
    qt_add_dbus_adaptor(kdenlive_SRCS org.kdenlive.MainWindow.xml mainwindow.h MainWindow)
endif()

## UI's
file(GLOB kdenlive_UIS "ui/*.ui")
ki18n_wrap_ui(kdenlive_UIS ${kdenlive_UIS})

add_library(kdenliveLib STATIC ${kdenlive_SRCS} ${kdenlive_UIS} definitions.h lib/localeHandling.cpp lib/localeHandling.h)

kconfig_target_kcfg_file(kdenliveLib
    FILE kdenlivesettings.kcfg
    CLASS_NAME KdenliveSettings
    MUTATORS
    SINGLETON

    GENERATE_PROPERTIES
    GENERATE_MOC

    QML_REGISTRATION

    # DEFAULT_VALUE_GETTERS
)
install(FILES kdenlivesettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})

ecm_add_qml_module(kdenliveLib
    URI org.kde.kdenlive
    GENERATE_PLUGIN_SOURCE
    DEPENDENCIES
      QtQuick
)

ecm_target_qml_sources(kdenliveLib
    SOURCES
    qml/ZoomBar.qml
    monitor/view/ProjectMonitor.qml
    monitor/view/Countdown.qml
    monitor/view/ClipMonitor.qml
    monitor/view/MonitorAutomask.qml
    monitor/view/MonitorCornerScene.qml
    monitor/view/MonitorGeometryScene.qml
    monitor/view/MonitorOverlay.qml
    monitor/view/MonitorRotoScene.qml
    monitor/view/MonitorSplit.qml
    monitor/view/MonitorSplitTracks.qml
    monitor/view/MonitorToolButton.qml
    monitor/view/MonitorTrimming.qml
    monitor/view/SceneToolBar.qml
    monitor/view/MultiScreenToolBar.qml
    monitor/view/EffectToolBar.qml
    monitor/view/MaskToolBar.qml
    monitor/view/MonitorRuler.qml
    monitor/view/OverlayStandard.qml
    monitor/view/OverlayMinimal.qml
    monitor/view/OverlayCenter.qml
    monitor/view/OverlayCenterDiagonal.qml
    monitor/view/OverlayThirds.qml
    monitor/view/OverlayLabel.qml
    monitor/view/ResizeHandle.qml
    monitor/view/ResizeLogic.js
    monitor/view/SnappingLogic.js
    monitor/view/RotationHandle.qml
    timeline2/view/qml/Timeline.qml
    timeline2/view/qml/TimelineLogic.js
    timeline2/view/qml/TrackHead.qml
    timeline2/view/qml/SubtitleTrackHead.qml
    timeline2/view/qml/AudioLevels.qml
    timeline2/view/qml/Track.qml
    timeline2/view/qml/Ruler.qml
    timeline2/view/qml/RulerZone.qml
    timeline2/view/qml/Clip.qml
    timeline2/view/qml/SubTitle.qml
    timeline2/view/qml/MixShape.qml
    timeline2/view/qml/ClipThumbs.qml
    timeline2/view/qml/ClipAudioThumbs.qml
    timeline2/view/qml/KeyframeView.qml
    timeline2/view/qml/Composition.qml
    timeline2/view/qml/PulsingAnimation.qml
    timeline2/view/qml/CornerSelectionShadow.qml
    assets/view/qml/AssetView.qml
    timeline2/view/qml/AssetMenu.qml
    timeline2/view/qml/GuidesMenu.qml
)

qt_add_resources(kdenlive_extra_SRCS icons.qrc uiresources.qrc)

## Icon for Windows and OSX
file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../data/icons/*-apps-kdenlive.png")
ecm_add_app_icon(kdenlive ICONS ${ICONS_SRCS}) # icon name = variable name

## Kdenlive Executable
add_executable(kdenlive main.cpp ${kdenlive_extra_SRCS} ${kdenlive})
target_include_directories(kdenlive
    PRIVATE ${CMAKE_BINARY_DIR}
    )
target_include_directories(kdenlive
	SYSTEM PRIVATE ${MLT_INCLUDE_DIR} ${MLTPP_INCLUDE_DIR})
target_link_libraries(kdenlive kdenliveLib kdenliveLibplugin)
install(TARGETS kdenlive DESTINATION ${KDE_INSTALL_BINDIR})

## Kdenlive Lib
target_include_directories(kdenliveLib PRIVATE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/external
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/timeline2/view/qml
    )
target_include_directories(kdenliveLib
	SYSTEM  PRIVATE ${MLT_INCLUDE_DIR} ${MLTPP_INCLUDE_DIR})
# Adds Qt definitions and include directories, and sets QT_LIBRARIES according
# to the components requested in find_package().
# KDE definitions and include directories *must* always come first, Qt follows
# (to avoid breaking builds when KDE and/or Qt are installed to different
# prefixes).
target_link_libraries(kdenliveLib PUBLIC
    KF${KF_MAJOR}::I18n
    KF${KF_MAJOR}::WidgetsAddons
    KF${KF_MAJOR}::Archive
    KF${KF_MAJOR}::CoreAddons
    KF${KF_MAJOR}::KIOCore
    KF${KF_MAJOR}::Codecs
    KF${KF_MAJOR}::KIOFileWidgets
    KF${KF_MAJOR}::KIOWidgets
    KF${KF_MAJOR}::NewStuffWidgets
    KF${KF_MAJOR}::NotifyConfig
    KF${KF_MAJOR}::XmlGui
    KF${KF_MAJOR}::GuiAddons
    KF${KF_MAJOR}::Notifications
    KF${KF_MAJOR}::TextWidgets
    KF${KF_MAJOR}::IconThemes
    KF${KF_MAJOR}::Solid
    KF${KF_MAJOR}::FileMetaData
    KF${KF_MAJOR}::Purpose
    KF${KF_MAJOR}::PurposeWidgets
    ${OPENGL_LIBRARIES}
    ${OPENGLES_LIBRARIES}
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES}
    OTIO::opentimelineio
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    kiss_fft
    )

if(KF6_VERSION VERSION_GREATER_EQUAL "6.8.0")
  target_link_libraries(kdenliveLib PUBLIC KF${KF_MAJOR}::I18nQml)
endif()

target_link_libraries(kdenliveLib PUBLIC
    Qt${QT_MAJOR_VERSION}::Svg
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::QuickControls2
    Qt${QT_MAJOR_VERSION}::QuickWidgets
    Qt${QT_MAJOR_VERSION}::Concurrent
    Qt${QT_MAJOR_VERSION}::Multimedia
    Qt${QT_MAJOR_VERSION}::NetworkAuth
    Qt${QT_MAJOR_VERSION}::SvgWidgets
    # Qt${QT_MAJOR_VERSION}::Test
)

target_include_directories(kdenliveLib SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(kdenliveLib PUBLIC ${FFMPEG_LIBRARIES})

if(WIN32)
    target_link_libraries(kdenliveLib PUBLIC d3d11 d3dcompiler)
endif()

if(USE_DBUS)
  target_link_libraries(kdenliveLib PUBLIC KF${KF_MAJOR}::DBusAddons)
endif()
if(BUILD_COVERAGE)
  target_link_libraries(kdenliveLib PUBLIC gcov)
endif()
set_property(TARGET kdenliveLib PROPERTY CXX_STANDARD 14)
add_definitions(${qt${QT_MAJOR_VERSION}gui_definitions} -DQT_STRICT_ITERATORS -DQT_NO_CAST_FROM_BYTEARRAY)

# Optional deps
if(CRASH_AUTO_TEST)
    target_compile_definitions(kdenliveLib PRIVATE CRASH_AUTO_TEST)
    if(TARGET RTTR::Core)
        target_link_libraries(kdenliveLib RTTR::Core)
    else()
        target_link_libraries(kdenliveLib RTTR::Core_Lib)
    endif()
endif()

if(DRMINGW_FOUND)
    target_compile_definitions(kdenlive PRIVATE -DUSE_DRMINGW)
    target_include_directories(kdenlive SYSTEM PRIVATE ${DRMINGW_INCLUDE_DIR})
    target_link_libraries(kdenlive ${DRMINGW_LIBRARY})
elseif(KF${KF_MAJOR}Crash_FOUND)
    target_compile_definitions(kdenlive PRIVATE -DKF5_USE_CRASH)
    target_link_libraries(kdenlive KF${KF_MAJOR}::Crash)
endif()

if(Q_WS_X11)
	target_include_directories(kdenliveLib SYSTEM PRIVATE ${X11_Xlib_INCLUDE_PATH})
    target_link_libraries(kdenliveLib PUBLIC ${X11_LIBRARIES})
endif()

if(SDL2_FOUND)
    target_link_libraries(kdenliveLib PUBLIC ${SDL2_LIBRARY})
elseif(SDL_FOUND)
    target_link_libraries(kdenliveLib PUBLIC ${SDL_LIBRARY})
endif()

if(LIBV4L2_FOUND)
    target_include_directories(kdenliveLib SYSTEM PRIVATE ${LIBV4L2_INCLUDE_DIR})
    target_link_libraries(kdenliveLib PUBLIC ${LIBV4L2_LIBRARY})
    target_compile_definitions(kdenliveLib PRIVATE -DUSE_V4L)
endif()

if(HAVE_LINUX_INPUT_H)
    target_compile_definitions(kdenliveLib PRIVATE -DUSE_JOGSHUTTLE)
    target_link_libraries(kdenliveLib PUBLIC media_ctrl)
endif()

if(APPLE)
    set_target_properties(kdenlive PROPERTIES MACOSX_BUNDLE ON)
endif()


if(NOT USE_DBUS)
    target_compile_definitions(kdenliveLib PRIVATE -DNODBUS)
    target_compile_definitions(kdenlive PRIVATE -DNODBUS)
endif()

if (BUILD_QCH)
    ecm_add_qch(
        Kdenlive_QCH
        NAME kdenlive
        VERSION ${KDENLIVE_VERSION}
        ORG_DOMAIN org.kde
        SOURCE_DIRS
            .
        MD_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md"
        TAGFILE_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        QCH_INSTALL_DESTINATION ${KDE_INSTALL_QTQCHDIR}
        COMPONENT Devel
    )
endif()

# TODO: enable once a few more warning are fixed
# add_dependencies(kdenlive all_qmllint)

ecm_finalize_qml_module(kdenliveLib
    DESTINATION ${KDE_INSTALL_QMLDIR})

