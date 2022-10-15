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

if (APPLE)
# Apple app package
set_target_properties(kdenlive PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist)
set_target_properties(kdenlive PROPERTIES
    MACOSX_BUNDLE_DISPLAY_NAME "Kdenlive"
    MACOSX_BUNDLE_BUNDLE_NAME "Kdenlive"
    MACOSX_BUNDLE_LONG_VERSION_STRING "Kdenlive ${RELEASE_SERVICE_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${RELEASE_SERVICE_VERSION}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.Kdenlive"
    MACOSX_BUNDLE_COPYRIGHT "2000-2022 The Kdenlive Authors")

endif()

install(FILES
  meta_magiclantern.png
  encodingprofiles.rc
  externalproxies.rc
  kdenliveeffectscategory.rc
  kdenlivetranscodingrc
  kdenlivedefaultlayouts.rc
  slideanimations.rc

  DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX})

install(FILES  kdenlive.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
install(FILES profiles.xml DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/export)
install(FILES org.kde.kdenlive.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES org.kde.kdenlive.desktop DESTINATION ${KDE_INSTALL_APPDIR})

find_package(SharedMimeInfo REQUIRED)
install(FILES org.kde.kdenlive.xml westley.xml DESTINATION ${KDE_INSTALL_MIMEDIR})
update_xdg_mimetypes(${KDE_INSTALL_MIMEDIR})

