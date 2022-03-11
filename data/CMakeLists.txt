add_subdirectory(effects)
add_subdirectory(generators)
add_subdirectory(transitions)
add_subdirectory(icons)
add_subdirectory(knewstuff)
add_subdirectory(lumas)
add_subdirectory(scripts)
add_subdirectory(man)
add_subdirectory(titles)
add_subdirectory(profiles)
add_subdirectory(resourceproviders)
add_subdirectory(shortcuts)
add_subdirectory(luts)

install(FILES
  meta_magiclantern.png
  encodingprofiles.rc
  externalproxies.rc
  kdenliveeffectscategory.rc
  kdenlivetranscodingrc
  kdenlivedefaultlayouts.rc
  slideanimations.rc

  DESTINATION ${DATA_INSTALL_DIR}${DATA_INSTALL_PREFIX})
install(FILES  kdenlive.notifyrc DESTINATION ${KNOTIFYRC_INSTALL_DIR})

install(FILES profiles.xml DESTINATION ${DATA_INSTALL_DIR}${DATA_INSTALL_PREFIX}/export)
install(FILES org.kde.kdenlive.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES org.kde.kdenlive.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})

find_package(SharedMimeInfo REQUIRED)
install(FILES org.kde.kdenlive.xml westley.xml DESTINATION ${XDG_MIME_INSTALL_DIR})
update_xdg_mimetypes(${XDG_MIME_INSTALL_DIR})

