# qtquickcontrols and qtwebchannel are required by qtwebengine
# "WebRTC" is required by "Pepper Plugins" is required by "Printing and PDF"
# don't use -I and -L switches as they will cause explicit link to
# OpenSSL instead of BoringSSL in QWebEngine
ExternalProject_Add(
  ext_qt
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://download.qt.io/official_releases/qt/5.11/5.11.2/single/qt-everywhere-src-5.11.2.tar.xz
  URL_MD5 152a8ade9c11fe33ff5bc95310a1bb64

  PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/qsql_mysql.diff

  CONFIGURE_COMMAND <SOURCE_DIR>/configure -prefix ${EXT_INSTALL_DIR} -opensource -confirm-license -verbose -nomake examples -nomake tests -nomake tools ICU_LIBDIR="${EXT_INSTALL_DIR}/lib" ICU_INCDIR="${EXT_INSTALL_DIR}/include" OPENSSL_LIBDIR="${EXT_INSTALL_DIR}/openssl/lib" OPENSSL_INCDIR="${EXT_INSTALL_DIR}/openssl/include" -openssl-linked -skip qt3d -skip qtactiveqt -skip qtcanvas3d -skip qtconnectivity -skip qtenginio -skip qtgraphicaleffects -skip qtlocation -skip qtwayland -skip qtandroidextras -skip qtserialport -skip qtdatavis3d -skip qtvirtualkeyboard -skip qtspeech -skip qtsensors -skip qtgamepad -skip qtscxml -skip qtremoteobjects -skip qtxmlpatterns -skip qtcharts -skip qtdatavis3d -skip qtgamepad -skip qtpurchasing -skip qtscxml -skip qtserialbus -skip qtspeech -skip qtvirtualkeyboard -skip qtdoc -skip qtmacextras -skip qtwinextras -skip qtwebglplugin -skip qtwebsockets -skip qtwebview -skip qtmultimedia -skip qtnetworkauth -skip qtimageformats -system-webengine-icu

  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
  DEPENDS ext_xslt ext_mysql ext_sqlite ext_postgresql ext_pcre2
)
