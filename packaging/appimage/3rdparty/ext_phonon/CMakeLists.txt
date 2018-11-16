ExternalProject_Add(
    ext_phonon
    DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
    URL https://download.kde.org/stable/phonon/4.10.1/phonon-4.10.1.tar.xz
    URL_MD5 edb9ab608c7b74b4f0f10ebcf53e050d

    CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
               -DPHONON_BUILD_PHONON4QT5=ON
)
