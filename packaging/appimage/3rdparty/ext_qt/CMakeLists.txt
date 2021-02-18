# qtquickcontrols and qtwebchannel are required by qtwebengine
# "WebRTC" is required by "Pepper Plugins" is required by "Printing and PDF"
# don't use -I and -L switches as they will cause explicit link to
# OpenSSL instead of BoringSSL in QWebEngine
ExternalProject_Add(
  ext_qt
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL http://download.qt.io/archive/qt/5.14/5.14.2/single/qt-everywhere-src-5.14.2.tar.xz
  URL_MD5 b3d2b6d00e6ca8a8ede6d1c9bdc74daf
  PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/qt-appimage-support.patch
  CMAKE_ARGS -DOPENSSL_LIBS='-L${EXT_INSTALL_DIR}/lib -lssl -lcrypto'
  BUILD_IN_SOURCE 1
  #PATCH_COMMAND ${PATCH_COMMAND} -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/qsql_mysql.diff
Â CONFIGURE_COMMAND <SOURCE_DIR>/configure ICU_LIBS="-I${EXT_INSTALL_DIR}/local/ -L${EXT_INSTALL_DIR}/local/lib -licui18n -licuuc -licudata" -prefix ${EXT_INSTALL_DIR} -opensource -confirm-license -openssl-linked -verbose -nomake examples -nomake tests -nomake tools -no-icu -skip qt3d -skip qtactiveqt -skip qtcanvas3d -skip qtconnectivity -skip qtlocation -skip qtwayland -skip qtwebchannel -skip qtwebengine -skip qtandroidextras -skip qtserialport -skip qtdatavis3d -skip qtvirtualkeyboard -skip qtspeech -skip qtsensors -skip qtgamepad -skip qtscxml -skip qtremoteobjects -skip qtxmlpatterns -skip qtcharts -skip qtpurchasing -skip qtserialbus -skip qtdoc -skip qtmacextras -skip qtwinextras -skip qtwebglplugin -skip qtwebsockets -skip qtwebview -linker gold

  BUILD_COMMAND $(MAKE)
  INSTALL_COMMAND $(MAKE) install
  DEPENDS ext_xslt ext_sqlite ext_pcre2
)
