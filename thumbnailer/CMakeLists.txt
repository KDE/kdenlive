kcoreaddons_add_plugin(mltpreview INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/thumbcreator")

target_sources(mltpreview PRIVATE mltpreview.cpp ../src/lib/localeHandling.cpp)

target_include_directories(mltpreview PRIVATE
  ${MLT_INCLUDE_DIR}
  ${MLTPP_INCLUDE_DIR}
)

target_link_libraries(mltpreview
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Gui
        KF5::KIOCore
        KF5::KIOGui
        ${MLT_LIBRARIES}
        ${MLTPP_LIBRARIES}
)
