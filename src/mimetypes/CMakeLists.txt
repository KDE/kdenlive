find_package(SharedMimeInfo REQUIRED)

install(FILES kdenlive.xml dv-video.xml hdv-video.xml DESTINATION ${XDG_MIME_INSTALL_DIR})
update_xdg_mimetypes(${XDG_MIME_INSTALL_DIR})