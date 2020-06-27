set(QT_DONT_USE_QTGUI 1)
set(QT_USE_QTDBUS 1)

include_directories(
    ${MLT_INCLUDE_DIR}
    ${MLTPP_INCLUDE_DIR}
)

set(kdenlive_render_SRCS
  kdenlive_render.cpp
  renderjob.cpp
  ../src/lib/localeHandling.cpp
)

add_executable(kdenlive_render ${kdenlive_render_SRCS})
ecm_mark_nongui_executable(kdenlive_render)

target_link_libraries(kdenlive_render Qt5::Core Qt5::Widgets Qt5::DBus Qt5::Xml
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES})


install(TARGETS kdenlive_render DESTINATION ${BIN_INSTALL_DIR})
