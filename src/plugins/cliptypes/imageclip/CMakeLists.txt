
SET(imageclip_SRCS
    imageclipplugin.cpp
    imageprojectclip.cpp
    imagetimelineclip.cpp
    imagetimelineclipitem.cpp
)

kde4_add_plugin(kdenliveimageclip ${imageclip_SRCS})


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


TARGET_LINK_LIBRARIES(kdenliveimageclip ${KDE4_KDECORE_LIBS} ${QT_LIBRARIES} ${LIBMLT_LIBRARY} ${LIBMLTPLUS_LIBRARY} kdenlivecore )

INSTALL( TARGETS kdenliveimageclip DESTINATION ${PLUGIN_INSTALL_DIR} )
INSTALL( FILES kdenliveimageclip.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
