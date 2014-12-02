set(westleypreview_SRCS westleypreview.cpp)

include_directories(
  ${KDE4_INCLUDES}
  ${MLT_INCLUDE_DIR}
  ${MLTPP_INCLUDE_DIR}
)

kde4_add_plugin(westleypreview ${westleypreview_SRCS})

#include(${QT_USE_FILE})

target_link_libraries(westleypreview
  ${KDE4_KIO_LIBS}
  ${QT_LIBRARIES}
  ${MLT_LIBRARIES}
  ${MLTPP_LIBRARIES}
)

install(TARGETS westleypreview DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES westleypreview.desktop DESTINATION ${SERVICES_INSTALL_DIR})
