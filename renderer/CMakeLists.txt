set(QT_DONT_USE_QTGUI 1)
set(QT_USE_QTDBUS 1)

include_directories(
    ${MLT_INCLUDE_DIR}
    ${MLTPP_INCLUDE_DIR}
)

set(kdenlive_render_SRCS
  kdenlive_render.cpp
  renderjob.cpp
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
add_executable(kdenlive_render ${kdenlive_render_SRCS})
ecm_mark_nongui_executable(kdenlive_render)

set(mlt_SRCS
  io.c
  melt.c
)
add_executable(kdenlive_melt ${mlt_SRCS})
ecm_mark_nongui_executable(kdenlive_melt)

target_link_libraries(kdenlive_melt
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES})


target_link_libraries(kdenlive_render Qt5::Core Qt5::DBus Qt5::Xml
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES})


install(TARGETS kdenlive_render DESTINATION ${BIN_INSTALL_DIR})
install(TARGETS kdenlive_melt DESTINATION ${BIN_INSTALL_DIR})
