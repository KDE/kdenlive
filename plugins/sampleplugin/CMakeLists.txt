set(sampleplugin_SRCS sampleplugin.cpp)

qt5_wrap_ui(sampleplugin_UIS countdown_ui.ui)

kde4_add_plugin(kdenlive_sampleplugin WITH_PREFIX
  ${sampleplugin_SRCS}
  ${sampleplugin_UIS}
)

add_definitions(${KDE4_DEFINITIONS})

include_directories(${KDE4_INCLUDES})

include(${QT_USE_FILE})

#add_definitions(${QT_DEFINITIONS})
#add_definitions(-DQT_NO_DEBUG)
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

target_link_libraries(kdenlive_sampleplugin
  ${KDE4_KDECORE_LIBS}
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${QT_LIBRARIES}
)

install(TARGETS kdenlive_sampleplugin DESTINATION ${PLUGIN_INSTALL_DIR})
