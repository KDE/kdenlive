set(westleypreview_SRCS westleypreview.cpp)

include_directories(
  ${KDE4_INCLUDES}
  ${LIBMLT_INCLUDE_DIR}
  ${LIBMLTPLUS_INCLUDE_DIR}
)

kde4_add_plugin(westleypreview ${westleypreview_SRCS})

include(${QT_USE_FILE})

target_link_libraries(westleypreview
  ${KDE4_KIO_LIBS}
  ${QT_LIBRARIES}
  ${LIBMLT_LIBRARY}
  ${LIBMLTPLUS_LIBRARY}
)

install(TARGETS westleypreview DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES westleypreview.desktop DESTINATION ${SERVICES_INSTALL_DIR})
