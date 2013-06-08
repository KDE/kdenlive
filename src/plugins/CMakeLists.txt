include_directories( ${CMAKE_SOURCE_DIR}/src/core )

add_subdirectory(cliptypes)
add_subdirectory(parameters)
add_subdirectory(projectparts)
add_subdirectory(timelineviewtools)

install(FILES kdenliveplugin.desktop DESTINATION ${SERVICETYPES_INSTALL_DIR})
