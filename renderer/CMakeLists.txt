

include_directories (
    ${QDBUS_INCLUDE_DIRS}
    ${KDE4_INCLUDES}
    ${CMAKE_SOURCE_DIR} 
    ${CMAKE_BINARY_DIR} 
)

LINK_LIBRARIES(
${LIBMLT_LIBRARY}
${LIBMLTPLUS_LIBRARY}
${optional_libs}
)

set(kdenlive_render_SRCS 
  kdenlive_render.cpp
  renderjob.cpp
)

kde4_add_executable(kdenlive_render ${kdenlive_render_SRCS})

target_link_libraries(kdenlive_render 
  ${QT_QTCORE_LIBRARY}
  ${QT_QTDBUS_LIBRARY}
)

install(TARGETS kdenlive_render DESTINATION ${BIN_INSTALL_DIR})

