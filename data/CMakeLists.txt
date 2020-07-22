add_subdirectory(effects)
add_subdirectory(generators)
add_subdirectory(transitions)
add_subdirectory(icons)
add_subdirectory(lumas)
add_subdirectory(man)
add_subdirectory(titles)
add_subdirectory(profiles)
add_subdirectory(shortcuts)

install(FILES
  banner.png
  encodingprofiles.rc
  externalproxies.rc
  metadata.properties
  meta_ffmpeg.png
  meta_libav.png
  meta_magiclantern.png
  timeline_athumbs.png
  timeline_avthumbs.png
  timeline_nothumbs.png
  timeline_vthumbs.png
  kdenliveeffectscategory.rc
  kdenlivetranscodingrc
  kdenlivedefaultlayouts.rc

  DESTINATION ${DATA_INSTALL_DIR}/kdenlive)
install(FILES  kdenlive.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR})
install(FILES kdenlive_renderprofiles.knsrc kdenlive_wipes.knsrc kdenlive_keyboardschemes.knsrc kdenlive_titles.knsrc DESTINATION ${CONFIG_INSTALL_DIR})
install(FILES profiles.xml DESTINATION ${DATA_INSTALL_DIR}/kdenlive/export)
install(FILES org.kde.kdenlive.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES org.kde.kdenlive.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

find_package(SharedMimeInfo REQUIRED)
install(FILES org.kde.kdenlive.xml westley.xml DESTINATION ${XDG_MIME_INSTALL_DIR})
update_xdg_mimetypes(${XDG_MIME_INSTALL_DIR})

