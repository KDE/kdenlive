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
  URL http://download.kde.org/stable/frameworks/5.51/extra-cmake-modules-5.51.0.zip
  URL_MD5 0c49de5686624746af45f578486c275f
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
)

ExternalProject_Add(
  ext_karchive
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/karchive-5.51.0.zip
  URL_MD5 a1df7fa0e563774582a141be902bf9e0
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kcodecs
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kcodecs-5.51.0.zip
  URL_MD5 690e3fc89a72a64ebf7d4dccfea07cb4
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kconfig
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kconfig-5.51.0.zip
  URL_MD5 efd15b025d74c6bdb7ba1f6c70eb59d3
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kdbusaddons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kdbusaddons-5.51.0.zip
  URL_MD5 a4e3f38596815004815e1d5912dcc9f7
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kcoreaddons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kcoreaddons-5.51.0.zip
  URL_MD5 bd29fb792d368dcc031277182e20dd57
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_sharedmimeinfo ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kguiaddons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kguiaddons-5.51.0.zip
  URL_MD5 a4ed02d971740b5a69d3bbd8078cd53c
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_ki18n
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/ki18n-5.51.0.zip
  URL_MD5 97ea04262c49403ba8c33323b367c362
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules ext_gettext
)

ExternalProject_Add(
  ext_kitemmodels
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kitemmodels-5.51.0.zip
  URL_MD5 82989ebe8c2c76df6fbd3bc4581923a8
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kitemviews
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kitemviews-5.51.0.zip
  URL_MD5 4b0ac783dd4a08b57442a6b349a29af0
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kwidgetsaddons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kwidgetsaddons-5.51.0.zip
  URL_MD5 069a7907a50a573f237b0ae07e143c31
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kwindowsystem
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kwindowsystem-5.51.0.zip
  URL_MD5 1cc9f16aef24588f56712beacea04173
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_solid
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/solid-5.51.0.zip
  URL_MD5 20928ec5ac4f24ac50d28e1f859564fc
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_sonnet
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/sonnet-5.51.0.zip
  URL_MD5 6da10e377b65fd0eb38a893f3b02bda4
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_syntaxhighlighting
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/syntax-highlighting-5.51.0.zip
  URL_MD5 960aa4322276d5591fe038351c420607
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kholidays
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://download.kde.org/stable/frameworks/5.51/kholidays-5.51.0.zip
  URL_MD5 41b208d432246eb8f012d55bc0c1b9ee
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules
)

ExternalProject_Add(
  ext_kactivities
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kactivities-5.51.0.zip
  URL_MD5 397fe66ad6b2bd4151de87b5bea27550
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_boost ext_kconfig ext_kcoreaddons ext_kwindowsystem
)

ExternalProject_Add(
  ext_kauth
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kauth-5.51.0.zip
  URL_MD5 c44908198ce80868a0f84d93bfbd2a0b
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kcoreaddons
)

ExternalProject_Add(
  ext_kcompletion
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kcompletion-5.51.0.zip
  URL_MD5 0538aaf30384c0ac0e69a245d4c99dc5
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kwidgetsaddons ext_kconfig
)

ExternalProject_Add(
  ext_kcrash
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kcrash-5.51.0.zip
  URL_MD5 4d424ec54f369eb6f6f8d9571e72d0f9
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kcoreaddons ext_kwindowsystem
)

ExternalProject_Add(
  ext_kjobwidgets
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kjobwidgets-5.51.0.zip
  URL_MD5 e352949fc004cad1a5df7ac6095e061f
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kcoreaddons ext_kwidgetsaddons
)

ExternalProject_Add(
  ext_knotifications
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/knotifications-5.51.0.zip
  URL_MD5 1727ef67b293228e783d70a8a04fb2b2
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kwindowsystem ext_kconfig ext_kcodecs ext_kcoreaddons ext_phonon
)

ExternalProject_Add(
  ext_kpackage
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kpackage-5.51.0.zip
  URL_MD5 7e04ab888adb565d6b3c7edab3f5f88b
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_karchive ext_ki18n ext_kcoreaddons
)

ExternalProject_Add(
  ext_kconfigwidgets
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kconfigwidgets-5.51.0.zip
  URL_MD5 943fe07904feaa3a8b05fe81733d1eb4
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kauth ext_kcoreaddons ext_kcodecs ext_kconfig ext_kguiaddons ext_ki18n ext_kwidgetsaddons
)

ExternalProject_Add(
  ext_kiconthemes
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kiconthemes-5.51.0.zip
  URL_MD5 6da17c5f2006b893025da305b1dfc011
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_karchive ext_ki18n ext_kcoreaddons ext_kconfigwidgets ext_kwidgetsaddons ext_kitemviews
)

ExternalProject_Add(
  ext_kservice
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kservice-5.51.0.zip
  URL_MD5 7444dbd3388a1e3cce9d8f7cd64c7d23
  PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/kservice.diff #this makes finding KCModules in AppImage possible
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kconfig ext_kcoreaddons ext_kcrash ext_kdbusaddons ext_ki18n
)

ExternalProject_Add(
  ext_kemoticons
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kemoticons-5.51.0.zip
  URL_MD5 f437001a8a5e401a8ac284cf554357ec
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_karchive ext_kconfig ext_kservice ext_kcoreaddons
)

ExternalProject_Add(
  ext_ktextwidgets
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/ktextwidgets-5.51.0.zip
  URL_MD5 2edb4f80800e47d6ecf52353decbdf10
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules ext_kcompletion ext_kconfig ext_kconfigwidgets ext_ki18n ext_kiconthemes ext_kservice ext_kwidgetsaddons ext_kwindowsystem ext_sonnet
)

ExternalProject_Add(
  ext_kglobalaccel
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kglobalaccel-5.51.0.zip
  URL_MD5 1cf9b73d7e909ed5899496eb52dfedc2
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kconfig ext_kcoreaddons ext_kcrash ext_kdbusaddons ext_kwindowsystem
)

ExternalProject_Add(
  ext_kxmlgui
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kxmlgui-5.51.0.zip
  URL_MD5 35555839af065708281ecf95f2cd9363
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kcoreaddons ext_kitemviews ext_kconfig ext_kconfigwidgets ext_ki18n ext_kiconthemes ext_ktextwidgets ext_kwidgetsaddons ext_kwindowsystem ext_kglobalaccel
)

ExternalProject_Add(
  ext_kbookmarks
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kbookmarks-5.51.0.zip
  URL_MD5 036f8e3e2d9c1cd537a275636569d1c6
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kconfig ext_kcoreaddons ext_kcodecs ext_kconfigwidgets ext_kiconthemes ext_kwidgetsaddons ext_kxmlgui
)

ExternalProject_Add(
  ext_kwallet
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kwallet-5.51.0.zip
  URL_MD5 da6f7e2282f6316b612cb0acaa7a140a
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_gcrypt ext_gpgme ext_kcoreaddons ext_kconfig ext_kwindowsystem ext_ki18n ext_kconfigwidgets ext_knotifications ext_kservice ext_kwidgetsaddons ext_kiconthemes ext_kdbusaddons
)

ExternalProject_Add(
  ext_kio
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kio-5.51.0.zip
  URL_MD5 ea59ac8326e5856b0ef1202c83d575b3
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_extra_cmake_modules ext_karchive ext_kconfig ext_kcoreaddons ext_kdbusaddons ext_ki18n ext_kservice ext_solid ext_kwindowsystem ext_kwidgetsaddons ext_kcompletion ext_kconfigwidgets ext_kiconthemes ext_kitemviews ext_kjobwidgets ext_kbookmarks ext_kwallet
)

ExternalProject_Add(
  ext_kdeclarative
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kdeclarative-5.51.0.zip
  URL_MD5 6d8b8a15b7d02ac662e7f22152e92f3f
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kconfig ext_ki18n ext_kiconthemes ext_kio ext_kwidgetsaddons ext_kwindowsystem ext_kglobalaccel ext_kguiaddons ext_kpackage
)

ExternalProject_Add(
  ext_kcmutils
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kcmutils-5.51.0.zip
  URL_MD5 4f548b92a27716f1d498b9da8887cf32
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kitemviews ext_kconfigwidgets ext_kcoreaddons ext_ki18n ext_kiconthemes ext_kservice ext_kxmlgui ext_kdeclarative
)

ExternalProject_Add(
  ext_kimageformats
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.kde.org/stable/frameworks/5.51/kimageformats-5.51.0.zip
  URL_MD5 bb889cbd5a3b39f226d57da4c694ff57
  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
  DEPENDS ext_kitemviews
)