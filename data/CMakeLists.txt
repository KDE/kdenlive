# SPDX-License-Identifier: BSD-2-Clause
# SPDX-FileCopyrightText: Vincent PINON <vincent.pinon@laposte.net>, Jean-Baptiste Mardelle <jb@kdenlive.org>, Julius Künzel <julius.kuenzel@kde.org>

add_subdirectory(effects)
add_subdirectory(generators)
add_subdirectory(layouts)
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
  slideanimations.rc
  clipjobsettings.rc

  DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX})

install(FILES kdenlive.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

install(FILES profiles.xml DESTINATION ${KDE_INSTALL_DATADIR}${DATA_INSTALL_PREFIX}/export)
install(FILES org.kde.kdenlive.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})
install(FILES org.kde.kdenlive.desktop DESTINATION ${KDE_INSTALL_APPDIR})

find_package(SharedMimeInfo REQUIRED)
install(FILES org.kde.kdenlive.xml westley.xml DESTINATION ${KDE_INSTALL_MIMEDIR})
update_xdg_mimetypes(${KDE_INSTALL_MIMEDIR})

