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
add_subdirectory(profiles)
add_subdirectory(project)
add_subdirectory(pythoninterfaces)
add_subdirectory(renderpresets)
add_subdirectory(scopes)
add_subdirectory(timeline2)
add_subdirectory(titler)
add_subdirectory(transitions)
add_subdirectory(utils)
add_subdirectory(widgets)
add_subdirectory(xml)

## toplevel source code files
set(kdenlive_SRCS
  ${kdenlive_SRCS}
  core.cpp
  definitions.cpp
  docktitlebarmanager.cpp
  layoutmanagement.cpp
  mainwindow.cpp
  mltconnection.cpp
  statusbarmessagelabel.cpp
  undohelper.cpp
  PARENT_SCOPE
)
if(CRASH_AUTO_TEST)
    set(kdenlive_SRCS ${kdenlive_SRCS} logger.cpp PARENT_SCOPE)
endif()

## Others special cases
kconfig_add_kcfg_files(kdenlive_SRCS kdenlivesettings.kcfgc)
install(FILES kdenlivesettings.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
ecm_qt_declare_logging_category(kdenlive_SRCS
    HEADER kdenlive_debug.h
    IDENTIFIER KDENLIVE_LOG
    CATEGORY_NAME org.kde.multimedia.kdenlive
    DESCRIPTION "kdenlive"
    EXPORT KDENLIVE
)

if(NOT NODBUS)
    if(USE_VERSIONLESS_TARGETS)
        qt_add_dbus_adaptor(kdenlive_SRCS org.kdenlive.MainWindow.xml mainwindow.h MainWindow)
    else()
        qt5_add_dbus_adaptor(kdenlive_SRCS org.kdenlive.MainWindow.xml mainwindow.h MainWindow)
    endif()
else()
    add_subdirectory(render)
endif()

## UI's
file(GLOB kdenlive_UIS "ui/*.ui")
ki18n_wrap_ui(kdenlive_UIS ${kdenlive_UIS})

## MOC
if(USE_VERSIONLESS_TARGETS)
    qt_wrap_cpp(kdenlive_MOC definitions.h)
else()
    qt5_wrap_cpp(kdenlive_MOC definitions.h)
endif()
set_property(SOURCE definitions.h PROPERTY SKIP_AUTOMOC ON)

add_library(kdenliveLib STATIC ${kdenlive_SRCS} ${kdenlive_UIS} ${kdenlive_MOC} lib/localeHandling.cpp lib/localeHandling.h)

if(USE_VERSIONLESS_TARGETS)
    qt_add_resources(kdenlive_extra_SRCS icons.qrc uiresources.qrc)
else()
    qt5_add_resources(kdenlive_extra_SRCS icons.qrc uiresources.qrc)
endif()

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
target_link_libraries(kdenlive kdenliveLib)
install(TARGETS kdenlive DESTINATION ${KDE_INSTALL_BINDIR})

install(FILES kdenliveui.rc DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/kdenlive)

## Kdenlive Lib
target_include_directories(kdenliveLib
    PRIVATE ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/external
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    )
target_include_directories(kdenliveLib
	SYSTEM  PRIVATE ${MLT_INCLUDE_DIR} ${MLTPP_INCLUDE_DIR})
# Adds Qt definitions and include directories, and sets QT_LIBRARIES according
# to the components requested in find_package().
# KDE definitions and include directories *must* always come first, Qt follows
# (to avoid breaking builds when KDE and/or Qt are installed to different
# prefixes).
target_link_libraries(kdenliveLib
    KF5::WidgetsAddons
    KF5::Archive
    KF5::CoreAddons
    KF5::KIOCore
    KF5::KIOFileWidgets
    KF5::KIOWidgets
    KF5::NotifyConfig
    KF5::NewStuff
    KF5::XmlGui
    KF5::GuiAddons
    KF5::Notifications
    KF5::TextWidgets
    KF5::Declarative
    KF5::IconThemes
    KF5::Solid
    ${OPENGL_LIBRARIES}
    ${OPENGLES_LIBRARIES}
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    kiss_fft)

if(USE_VERSIONLESS_TARGETS)
    target_link_libraries(kdenliveLib
        Qt::Svg
        Qt::Quick
        Qt::QuickControls2
        Qt::QuickWidgets
        Qt::Concurrent
        Qt::Multimedia
        Qt::NetworkAuth)
else()
    target_link_libraries(kdenliveLib
        Qt${QT_MAJOR_VERSION}::Svg
        Qt${QT_MAJOR_VERSION}::Quick
        Qt${QT_MAJOR_VERSION}::QuickControls2
        Qt${QT_MAJOR_VERSION}::QuickWidgets
        Qt${QT_MAJOR_VERSION}::Concurrent
        Qt${QT_MAJOR_VERSION}::Multimedia
        Qt${QT_MAJOR_VERSION}::NetworkAuth)
endif()
if (QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(kdenliveLib Qt${QT_MAJOR_VERSION}::SvgWidgets)
endif()

if(NOT NODBUS)
  target_link_libraries(kdenliveLib KF5::DBusAddons)
endif()
if(BUILD_COVERAGE)
  target_link_libraries(kdenliveLib gcov)
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
elseif(KF5Crash_FOUND)
    target_compile_definitions(kdenlive PRIVATE -DKF5_USE_CRASH)
    target_link_libraries(kdenlive KF5::Crash)
endif()

if(KF5_FILEMETADATA)
    target_compile_definitions(kdenliveLib PRIVATE -DKF5_USE_FILEMETADATA)
    target_link_libraries(kdenliveLib KF5::FileMetaData)
endif()

if(KF5_PURPOSE)
    target_compile_definitions(kdenliveLib PRIVATE -DKF5_USE_PURPOSE)
    target_link_libraries(kdenliveLib KF5::Purpose KF5::PurposeWidgets)
endif()

if(Q_WS_X11)
	target_include_directories(kdenliveLib SYSTEM PRIVATE ${X11_Xlib_INCLUDE_PATH})
    target_link_libraries(kdenliveLib ${X11_LIBRARIES})
endif()

if(SDL2_FOUND)
    target_link_libraries(kdenliveLib ${SDL2_LIBRARY})
elseif(SDL_FOUND)
    target_link_libraries(kdenliveLib ${SDL_LIBRARY})
endif()

if(LIBV4L2_FOUND)
    target_include_directories(kdenliveLib SYSTEM PRIVATE ${LIBV4L2_INCLUDE_DIR})
    target_link_libraries(kdenliveLib ${LIBV4L2_LIBRARY})
    target_compile_definitions(kdenliveLib PRIVATE -DUSE_V4L)
endif()

if(HAVE_LINUX_INPUT_H)
    target_compile_definitions(kdenliveLib PRIVATE -DUSE_JOGSHUTTLE)
    target_link_libraries(kdenliveLib media_ctrl)
endif()

if(NODBUS)
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

