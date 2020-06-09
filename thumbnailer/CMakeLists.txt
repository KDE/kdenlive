set(mltpreview_SRCS mltpreview.cpp ../src/lib/localeHandling.cpp)

include_directories(
  ${MLT_INCLUDE_DIR}
  ${MLTPP_INCLUDE_DIR}
)

add_library(mltpreview MODULE ${mltpreview_SRCS})


target_link_libraries(mltpreview
        Qt5::Core
        Qt5::Gui
        KF5::KIOCore
        KF5::KIOWidgets
  ${MLT_LIBRARIES}
  ${MLTPP_LIBRARIES}
)
#FIXME: For some reason the plugin only seems to work if installed in $lib/qt5/plugins and not in $lib/plugins like KF5 recommends
install(TARGETS mltpreview DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES mltpreview.desktop DESTINATION ${SERVICES_INSTALL_DIR})
