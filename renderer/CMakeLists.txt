set(QT_DONT_USE_QTGUI 1)
if(NOT NODBUS)
set(QT_USE_QTDBUS 1)
endif()
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

target_link_libraries(kdenlive_render Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Widgets Qt${QT_MAJOR_VERSION}::Xml
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES})
if(NODBUS)
    target_compile_definitions(kdenlive_render PRIVATE NODBUS)
    target_link_libraries(kdenlive_render Qt${QT_MAJOR_VERSION}::Network)
else()
    target_link_libraries(kdenlive_render Qt${QT_MAJOR_VERSION}::DBus)
endif()

install(TARGETS kdenlive_render DESTINATION ${BIN_INSTALL_DIR})
