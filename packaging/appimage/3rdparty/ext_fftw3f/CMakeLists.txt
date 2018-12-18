SET(PREFIX_ext_fftw3f "${EXTPREFIX}" )
ExternalProject_Add( ext_fftw3f
        DOWNLOAD_DIR ${EXTERNALS_DOWNLOAD_DIR}
        URL http://www.fftw.org/fftw-3.3.7.tar.gz
        URL_MD5 0d5915d7d39b3253c1cc05030d79ac47

    CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE} -DENABLE_FLOAT=ON -DENABLE_THREADS=ON
)
