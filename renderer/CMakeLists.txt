SET (RENDERER_SRC kdenlive_renderer.c)

ADD_EXECUTABLE(kdenlive_renderer ${RENDERER_SRC})
TARGET_LINK_LIBRARIES(kdenlive_renderer mlt )
INSTALL (PROGRAMS kdenlive_renderer DESTINATION ${CMAKE_INSTALL_PREFIX}/bin )
