#first straigth forward until we have a doc translation
IF (KDE3_MEINPROC_EXECUTABLE)
    FILE(GLOB docimages *.png)
    ADD_CUSTOM_COMMAND(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/index.cache.bz2
        COMMAND
        ${KDE3_MEINPROC_EXECUTABLE}
        ARGS "--check" "--cache" "${CMAKE_CURRENT_SOURCE_DIR}/index.cache.bz2"
        ${CMAKE_CURRENT_SOURCE_DIR}/index.docbook
        DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/index.docbook
        ${docimages}
    )
    INSTALL(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/index.cache.bz2
        DESTINATION ${HTML_INSTALL_DIR}/de/kdenlive
        RENAME
        index.cache.bz2
    )
    INSTALL(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/index.docbook
        ${docimages}
        DESTINATION ${HTML_INSTALL_DIR}/de/kdenlive
    )
    # creates the link to the common-section during install
    # INSTALL(SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/makelink.cmake)
    ADD_CUSTOM_TARGET(doc ALL
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/index.cache.bz2)

#${CMAKE_CURRENT_BINARY_DIR}/common)

ENDIF (KDE3_MEINPROC_EXECUTABLE)