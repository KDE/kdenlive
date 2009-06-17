

########### next target ###############
include_directories (
    ${KDE4_INCLUDES}
    ${LIBMLT_INCLUDE_DIR}
    ${LIBMLTPLUS_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR} 
    ${CMAKE_BINARY_DIR} 
)

LINK_LIBRARIES(
    ${LIBMLT_LIBRARY}
    ${LIBMLTPLUS_LIBRARY}
)

set(westleypreview_PART_SRCS westleypreview.cpp)

kde4_add_plugin(westleypreview ${westleypreview_PART_SRCS})

target_link_libraries(westleypreview ${KDE4_KIO_LIBS} ${LIBMLTPLUS_LIBRARY} ${LIBMLT_LIBRARY})

install(TARGETS westleypreview DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install(FILES westleypreview.desktop DESTINATION ${SERVICES_INSTALL_DIR})

