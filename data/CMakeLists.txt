add_subdirectory(effects)
add_subdirectory(icons)
add_subdirectory(lumas)
add_subdirectory(man)
add_subdirectory(titles)
install(FILES
  banner.png
  blacklisted_effects.txt
  blacklisted_transitions.txt
  encodingprofiles.rc
  kdenliveui.rc
  kdenlive.notifyrc
  metadata.properties
  meta_ffmpeg.png
  meta_libav.png
  meta_magiclantern.png
  timeline_athumbs.png
  timeline_avthumbs.png
  timeline_nothumbs.png
  timeline_vthumbs.png
  DESTINATION ${DATA_INSTALL_DIR}/kdenlive)
install(FILES profiles.xml DESTINATION ${DATA_INSTALL_DIR}/kdenlive/export)
install(FILES
  kdenliveeffectscategory.rc
  kdenlive_projectprofiles.knsrc
  kdenlive_renderprofiles.knsrc
  kdenlive_titles.knsrc
  kdenlive_wipes.knsrc
  kdenlivetranscodingrc
  DESTINATION ${CONFIG_INSTALL_DIR})
install(FILES kdenlive.xpm DESTINATION share/pixmaps)
install(FILES kdenlive.menu DESTINATION share/menu RENAME kdenlive)
install(FILES kdenlive.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

find_package(SharedMimeInfo REQUIRED)
install(FILES kdenlive.xml westley.xml DESTINATION ${XDG_MIME_INSTALL_DIR})
update_xdg_mimetypes(${XDG_MIME_INSTALL_DIR})

