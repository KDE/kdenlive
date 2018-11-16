set(EXT_INSTALL_DIR_applications "${EXT_INSTALL_DIR}")

ExternalProject_Add(
  ext_akonadi
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://download.kde.org/stable/applications/18.08.2/src/akonadi-18.08.2.tar.xz
  URL_MD5 bff15765460e954105217831f2786dc2

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}

  DEPENDS ext_sharedmimeinfo ext_boost ext_xslt ext_kcompletion ext_kconfig ext_kconfigwidgets ext_kcoreaddons ext_kdbusaddons ext_ki18n ext_kiconthemes ext_kitemmodels ext_kitemviews ext_kio ext_kwidgetsaddons ext_kwindowsystem ext_kxmlgui ext_kcrash
)

ExternalProject_Add(
  ext_kpimtextedit
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://download.kde.org/stable/applications/18.08.2/src/kpimtextedit-18.08.2.tar.xz
  URL_MD5 a360356a13ea0ee0f179fa28f267c1d8

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}

  DEPENDS ext_kcodecs ext_kconfig ext_kconfigwidgets ext_kcoreaddons ext_kemoticons ext_ki18n ext_kiconthemes ext_kio ext_sonnet ext_syntaxhighlighting ext_kwidgetsaddons ext_kxmlgui ext_grantlee
)

ExternalProject_Add(
  ext_kidentitymanagement
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://download.kde.org/stable/applications/18.08.2/src/kidentitymanagement-18.08.2.tar.xz
  URL_MD5 c7061788d60a167a3ef92cb5fd7a3ddd

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}

  DEPENDS ext_kcoreaddons ext_kcompletion ext_ktextwidgets ext_kxmlgui ext_kio ext_kconfig ext_kcodecs ext_kiconthemes ext_kpimtextedit
)

ExternalProject_Add(
  ext_kcontacts
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://download.kde.org/stable/applications/18.08.2/src/kcontacts-18.08.2.tar.xz
  URL_MD5 9686d42582cdf6b2a931d2ae036c1433

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}

  DEPENDS ext_kcoreaddons ext_ki18n ext_kconfig ext_kcodecs
)