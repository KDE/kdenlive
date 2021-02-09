# Create a static library from oauth2
if(Qt5WebEngineWidgets_FOUND)
ki18n_wrap_ui(login_UI logindialog_ui.ui)
add_library(qt-oauth-lib STATIC oauth2.cpp logindialog.cpp ${login_UI})
target_link_libraries(qt-oauth-lib Qt5::WebEngineWidgets KF5::I18n KF5::ConfigGui)
target_compile_definitions(qt-oauth-lib PRIVATE -DQT5_USE_WEBENGINE)
endif()
