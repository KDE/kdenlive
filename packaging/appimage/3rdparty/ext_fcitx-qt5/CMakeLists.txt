ExternalProject_Add(
    ext_fcitx-qt5
    DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
    URL https://github.com/fcitx/fcitx-qt5/archive/refs/tags/1.2.5.tar.gz
    URL_MD5 5a2aa2ce28d766cdfcd7465699bae996

    SOURCE_DIR fcitx-qt5-1.2.5
    INSTALL_DIR ${EXT_INSTALL_DIR}
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXT_INSTALL_DIR} -DCMAKE_BUILD_TYPE=${GLOBAL_BUILD_TYPE} ${GLOBAL_PROFILE} -DENABLE_LIBRARY=OFF

    DEPENDS ext_qt
    UPDATE_COMMAND ""
)
