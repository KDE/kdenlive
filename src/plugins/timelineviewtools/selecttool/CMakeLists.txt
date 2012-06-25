
SET(selecttool_SRCS
  selectclipitemtool.cpp
  selecttoolplugin.cpp
)

kde4_add_plugin(kdenliveselecttool ${selecttool_SRCS})


# KDE definitions and include directories *must* always come first, Qt follows
# (to avoid breaking builds when KDE and/or Qt are installed to different
# prefixes).
add_definitions(${KDE4_DEFINITIONS})

include_directories(
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${KDE4_INCLUDES} # Adds Qt include directories too.
  ${LIBMLT_INCLUDE_DIR}
  ${LIBMLTPLUS_INCLUDE_DIR}
)

# Adds Qt definitions and include directories, and sets QT_LIBRARIES according
# to the components requested in find_package().
include(${QT_USE_FILE})


TARGET_LINK_LIBRARIES(kdenliveselecttool ${KDE4_KDECORE_LIBS} ${KDE4_KDEUI_LIBS} ${QT_LIBRARIES} kdenlivecore )

INSTALL( TARGETS kdenliveselecttool DESTINATION ${PLUGIN_INSTALL_DIR} )
INSTALL( FILES kdenliveselecttool.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
install(FILES selecttool_ui.rc DESTINATION ${DATA_INSTALL_DIR}/kdenlive)


