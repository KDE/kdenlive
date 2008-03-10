project(kdenlive)


set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules )

# search packages used by KDE
find_package(KDE4 REQUIRED)
include (KDE4Defaults)
include (MacroLibrary)
include(MacroOptionalAddSubdirectory)

add_definitions (${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories (${QDBUS_INCLUDE_DIRS}  ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES} ${CMAKE_SOURCE_DIR}/widgets)

FIND_PACKAGE(LIBMLT REQUIRED)
FIND_PACKAGE(LIBMLTPLUS REQUIRED)
FIND_PACKAGE(LIBFFMPEG REQUIRED)

add_subdirectory(src src/cmake_bindir)
add_subdirectory(renderer renderer/cmake_bindir)
add_subdirectory(effects )
add_subdirectory(export ) 