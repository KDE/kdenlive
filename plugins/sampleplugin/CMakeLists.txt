
include_directories (
    ${CMAKE_SOURCE_DIR} 
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${KDE4_INCLUDES}
)

add_definitions( ${KDE4_DEFINITIONS} )

#add_definitions (${QT_DEFINITIONS})
ADD_DEFINITIONS(-DQT_PLUGIN)
#ADD_DEFINITIONS(-DQT_NO_DEBUG)
ADD_DEFINITIONS(-DQT_SHARED)

kde4_add_ui_files(plugin_UI countdown_ui.ui)

SET(sampleplugindir_SRCS
        sampleplugin.cpp)

KDE4_ADD_PLUGIN(kdenlive_sampleplugin WITH_PREFIX ${sampleplugindir_SRCS} ${plugin_UI})
#ADD_LIBRARY(kdenlive_sampleplugin SHARED ${sampleplugindir_SRCS})

TARGET_LINK_LIBRARIES(kdenlive_sampleplugin ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} ${KDE4_KDEUI_LIBS} ${KDE4_KDECORE_LIBS} ${KDE4_KIO_LIBS})

INSTALL(TARGETS kdenlive_sampleplugin DESTINATION ${PLUGIN_INSTALL_DIR})

