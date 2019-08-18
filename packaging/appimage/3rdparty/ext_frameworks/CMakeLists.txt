# All needed frameworks:
# Tier 1
#   Archive
#   Codecs
#   Config
#   CoreAddons
#   DBusAddons
#   GuiAddons
#   I18n
#   ItemModels
#   ItemViews
#   WidgetsAddons
#   WindowSystem
#   Solid
#   Sonnet
#   SyntaxHighlighting
# Tier 2
#   Activities
#   Auth
#   Completion
#   Crash
#   JobWidgets
#   Notifications
#   Package
# Tier 3
#   Emoticons
#   ConfigWidgets
#   IconThemes
#   Service
#   TextWidgets
#   XmlGui
#   Bookmarks
#   GlobalAccel
#   KIO
#   Declarative
#   KCMUtils
#   Wallet

ExternalProject_Add(
  ext_extra_cmake_modules
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/extra-cmake-modules-5.61.0.zip
  URL_MD5 e63d81fc203c03667cc9043395c1e80d
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
)

ExternalProject_Add(
  ext_karchive
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/karchive-5.61.0.zip
  URL_MD5 24f9b0c849943e9a7cdee48facaf62ce
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kcodecs
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kcodecs-5.61.0.zip
  URL_MD5 6c2615ec49c224adea10785e50cb21c5
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kconfig
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kconfig-5.61.0.zip
  URL_MD5 0fedc596e49c82647853710582eb9138
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kdbusaddons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kdbusaddons-5.61.0.zip
  URL_MD5 837514e0b634916221cbfa5705cb1aca
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kcoreaddons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kcoreaddons-5.61.0.zip
  URL_MD5 62e72d0e2cb1a24de6b10eebea43fe48
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_sharedmimeinfo ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kguiaddons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kguiaddons-5.61.0.zip
  URL_MD5 0236f2bb13b692362b0d875baa06e8ac
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_ki18n
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/ki18n-5.61.0.zip
  URL_MD5 2ce768761b8b1e1d2f52cbea143884ce
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules ext_gettext
)

ExternalProject_Add(
  ext_kitemmodels
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kitemmodels-5.61.0.zip
  URL_MD5 32866b0c6b0313cc82f11cf99d3c4d2b
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kitemviews
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kitemviews-5.61.0.zip
  URL_MD5 4f5f2b12e29eb7d810f984d45a9c5964
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kwidgetsaddons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kwidgetsaddons-5.61.0.zip
  URL_MD5 50bf68ff09fe30d61b2756ff2a3b9818
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kwindowsystem
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kwindowsystem-5.61.0.zip
  URL_MD5 9e2d29a266ca085487038403e1514bbc
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_solid
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/solid-5.61.0.zip
  URL_MD5 0a842b1c78c9482d719c07e37ae7f333
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_sonnet
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/sonnet-5.61.0.zip
  URL_MD5 185c918e14898f1815a4d1fe62db83d7
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_syntaxhighlighting
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/syntax-highlighting-5.61.0.zip
  URL_MD5 f31399bd41691f015b2b61373719b96a
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kholidays
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://download.kde.org/stable/frameworks/5.61/kholidays-5.61.0.zip
  URL_MD5 fdd9193fcd666a76d9d1d63470a2aeba
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kactivities
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kactivities-5.61.0.zip
  URL_MD5 582e7738a8a7c4dc736c4c561f2b90cd
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_boost ext_kconfig ext_kcoreaddons ext_kwindowsystem
)

ExternalProject_Add(
  ext_kauth
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kauth-5.61.0.zip
  URL_MD5 2b098e9374d7bcc1ef640f2e98c944b4
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kcoreaddons
)

ExternalProject_Add(
  ext_kcompletion
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kcompletion-5.61.0.zip
  URL_MD5 9d3e10ccda7e988a60e59ab87eaa0e56
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kwidgetsaddons ext_kconfig
)

ExternalProject_Add(
  ext_kcrash
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kcrash-5.61.0.zip
  URL_MD5 3e94cb696f2ae8ddda21b653d961d686
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kcoreaddons ext_kwindowsystem
)

ExternalProject_Add(
  ext_kjobwidgets
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kjobwidgets-5.61.0.zip
  URL_MD5 d1741558f0bdc7aee4eb00a63a7ad0bb
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kcoreaddons ext_kwidgetsaddons
)

ExternalProject_Add(
  ext_knotifications
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/knotifications-5.61.0.zip
  URL_MD5 ea46fbd930228511743d0793fd4aede9
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kwindowsystem ext_kconfig ext_kcodecs ext_kcoreaddons ext_phonon
)

ExternalProject_Add(
  ext_attica
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/attica-5.61.0.zip
  URL_MD5 50143e5c131a94fd383ed8fffc03f793
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kwindowsystem ext_kconfig ext_kcodecs ext_kcoreaddons ext_phonon
)

ExternalProject_Add(
  ext_kpackage
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kpackage-5.61.0.zip
  URL_MD5 a7c324a083a95bdbf48362995ed72b18
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_karchive ext_ki18n ext_kcoreaddons
)

ExternalProject_Add(
  ext_kconfigwidgets
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kconfigwidgets-5.61.0.zip
  URL_MD5 c65357e83034ac247405f03a61536da8
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kauth ext_kcoreaddons ext_kcodecs ext_kconfig ext_kguiaddons ext_ki18n ext_kwidgetsaddons
)

ExternalProject_Add(
  ext_kiconthemes
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kiconthemes-5.61.0.zip
  URL_MD5 bee44c7d4de4f6c44ea6f898d1447907
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_karchive ext_ki18n ext_kcoreaddons ext_kconfigwidgets ext_kwidgetsaddons ext_kitemviews
)

ExternalProject_Add(
  ext_kservice
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kservice-5.61.0.zip
  URL_MD5 a89621b85f005519b6e8bb9dbcea6e81
  #PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/kservice.diff #this makes finding KCModules in AppImage possible
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kconfig ext_kcoreaddons ext_kcrash ext_kdbusaddons ext_ki18n
)

ExternalProject_Add(
  ext_kemoticons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kemoticons-5.61.0.zip
  URL_MD5 2ee202a4d8421d7e7e3a7ead0d0ac8cc
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_karchive ext_kconfig ext_kservice ext_kcoreaddons
)

ExternalProject_Add(
  ext_ktextwidgets
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/ktextwidgets-5.61.0.zip
  URL_MD5 d806865de5f32a50ce985f6b6e1d7fc2
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules ext_kcompletion ext_kconfig ext_kconfigwidgets ext_ki18n ext_kiconthemes ext_kservice ext_kwidgetsaddons ext_kwindowsystem ext_sonnet
)

ExternalProject_Add(
  ext_kglobalaccel
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kglobalaccel-5.61.0.zip
  URL_MD5 3da7c0e1fe47e92993ef275b0c7512a4
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kconfig ext_kcoreaddons ext_kcrash ext_kdbusaddons ext_kwindowsystem
)

ExternalProject_Add(
  ext_kxmlgui
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kxmlgui-5.61.0.zip
  URL_MD5 37a6bbc26dc1ebad278c0bc0a63c5af5
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kcoreaddons ext_kitemviews ext_kconfig ext_kconfigwidgets ext_ki18n ext_kiconthemes ext_ktextwidgets ext_kwidgetsaddons ext_kwindowsystem ext_kglobalaccel
)

ExternalProject_Add(
  ext_kbookmarks
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kbookmarks-5.61.0.zip
  URL_MD5 3b067a8dbe4784ff3f229767f6e355db
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kconfig ext_kcoreaddons ext_kcodecs ext_kconfigwidgets ext_kiconthemes ext_kwidgetsaddons ext_kxmlgui
)

ExternalProject_Add(
  ext_kwallet
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kwallet-5.61.0.zip
  URL_MD5 97f6af2b3c756a9b7c49967e744bac99
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_gcrypt ext_gpgme ext_kcoreaddons ext_kconfig ext_kwindowsystem ext_ki18n ext_kconfigwidgets ext_knotifications ext_kservice ext_kwidgetsaddons ext_kiconthemes ext_kdbusaddons
)

ExternalProject_Add(
  ext_kio
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kio-5.61.0.zip
  URL_MD5 98f9cf494c281faae412991c5e99a94b
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules ext_karchive ext_kconfig ext_kcoreaddons ext_kdbusaddons ext_ki18n ext_kservice ext_solid ext_kwindowsystem ext_kwidgetsaddons ext_kcompletion ext_kconfigwidgets ext_kiconthemes ext_kitemviews ext_kjobwidgets ext_kbookmarks ext_kwallet
)

ExternalProject_Add(
  ext_knewstuff
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/knewstuff-5.61.0.zip
  URL_MD5 fe4a4a674abb1588691fc76030109f81
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kio ext_kwindowsystem ext_kconfig ext_kcodecs ext_kcoreaddons ext_phonon ext_attica
)

ExternalProject_Add(
  ext_knotifyconfig
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/knotifyconfig-5.61.0.zip
  URL_MD5 c5ab95425c6864a2ccb1af784c0d2e84
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kwindowsystem ext_kconfig ext_kcodecs ext_kcoreaddons ext_phonon ext_knewstuff
)

ExternalProject_Add(
  ext_kdeclarative
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kdeclarative-5.61.0.zip
  URL_MD5 228c640eb598d59ff7ff031c84f1ace7
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kconfig ext_ki18n ext_kiconthemes ext_kio ext_kwidgetsaddons ext_kwindowsystem ext_kglobalaccel ext_kguiaddons ext_kpackage
)

ExternalProject_Add(
  ext_kcmutils
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kcmutils-5.61.0.zip
  URL_MD5 4c41ec7dfd8656199ce83d13ef9ab72f
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kitemviews ext_kconfigwidgets ext_kcoreaddons ext_ki18n ext_kiconthemes ext_kservice ext_kxmlgui ext_kdeclarative
)

ExternalProject_Add(
  ext_kimageformats
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kimageformats-5.61.0.zip
  URL_MD5 fb09577e4eeb4971147cad6ab69c6d75
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kitemviews
)

ExternalProject_Add(
  ext_kpurpose
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/purpose-5.61.0.zip
  URL_MD5 32270091283bcab0452ccf79a76128c2
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kxmlgui
)

ExternalProject_Add(
  ext_kfilemetadata
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kfilemetadata-5.61.0.zip
  URL_MD5 95ccb70f3d00e1a203239076bad6bf63
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kguiaddons
)

ExternalProject_Add(
  ext_kdoctools
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/kdoctools-5.61.0.zip
  URL_MD5 c522e8c468fce403c5284e9c61e5b458
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kcoreaddons
)

ExternalProject_Add(
  ext_breezeicons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.61/breeze-icons-5.61.0.zip
  URL_MD5 0c7199da9510bd8fd59fd6a6bc8b8c78
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
)

ExternalProject_Add(
  ext_kdecoration
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/plasma/5.16.4/kdecoration-5.16.4.tar.xz
  URL_MD5 5e44ce8c340a2776d0c26c01e052de98
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
)

ExternalProject_Add(
  ext_breeze
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/plasma/5.16.4/breeze-5.16.4.tar.xz
  URL_MD5 b9669c56c282a3a99ad8b5c8ab9e28e7
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kdecoration
)
