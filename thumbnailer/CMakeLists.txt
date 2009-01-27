

########### next target ###############
include_directories (
    ${KDE4_INCLUDES}
    ${CMAKE_SOURCE_DIR} 
    ${CMAKE_BINARY_DIR} 
)


set(westleypreview_PART_SRCS westleypreview.cpp)

kde4_add_plugin(westleypreview ${westleypreview_PART_SRCS})

target_link_libraries(westleypreview ${KDE4_KIO_LIBS})

install(TARGETS westleypreview DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install(FILES westleypreview.desktop DESTINATION ${SERVICES_INSTALL_DIR})

