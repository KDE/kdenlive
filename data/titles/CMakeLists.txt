FILE(GLOB files "${CMAKE_CURRENT_SOURCE_DIR}/*.kdenlivetitle")
INSTALL (FILES ${files} DESTINATION ${DATA_INSTALL_DIR}/kdenlive/titles)
