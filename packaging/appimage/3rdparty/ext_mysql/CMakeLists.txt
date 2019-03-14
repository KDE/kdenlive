ExternalProject_Add(
  ext_mysql
  DOWNLOAD_DIR ${EXT_DOWNLOAD_DIR}
  URL https://dev.mysql.com/get/Downloads/MySQL-8.0/mysql-boost-8.0.12.tar.gz
  URL_MD5 82ba7cc22755f77635768c6e6d184161

  CMAKE_ARGS ${CMAKE_GLOBAL_PROFILE}
              -DWITH_BOOST=<SOURCE_DIR>/boost
              -DBUILD_CONFIG=mysql_release
              -DWITH_LZMA=system
              -DWITH_ICU=system
              -DINSTALL_STATIC_LIBRARIES=OFF
              -DIGNORE_AIO_CHECK=1
#               -DCPACK_MONOLITHIC_INSTALL=1


  BUILD_COMMAND "" # on default whole server would be built
  INSTALL_COMMAND ""
  DEPENDS ext_ncurses ext_openssl
)

# build and install client only
ExternalProject_Add_Step(
ext_mysql client
COMMAND ${CMAKE_MAKE_PROGRAM}
COMMAND ${CMAKE_MAKE_PROGRAM} install
DEPENDEES install
WORKING_DIRECTORY <BINARY_DIR>/libmysql
)

ExternalProject_Add_Step(
ext_mysql scripts
COMMAND ${CMAKE_MAKE_PROGRAM}
COMMAND ${CMAKE_MAKE_PROGRAM} install
DEPENDEES client
WORKING_DIRECTORY <BINARY_DIR>/scripts
)

# build and install headers
ExternalProject_Add_Step(
ext_mysql headers
COMMAND ${CMAKE_MAKE_PROGRAM}
COMMAND ${CMAKE_MAKE_PROGRAM} install
DEPENDEES scripts
WORKING_DIRECTORY <BINARY_DIR>/include)
