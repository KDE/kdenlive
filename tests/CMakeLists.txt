############################
# Tests
############################

set(QT_MIN_VERSION 5.5.0)
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fexceptions")


SET(Tests_SRCS
    TestMain.cpp
    modeltest.cpp
    groupstest.cpp
    snaptest.cpp
    regressions.cpp
    ../src/doc/docundostack.cpp
    ../src/timeline2/model/clipmodel.cpp
    ../src/timeline2/model/compositionmodel.cpp
    ../src/timeline2/model/groupsmodel.cpp
    ../src/timeline2/model/timelineitemmodel.cpp
    ../src/timeline2/model/timelinemodel.cpp
    ../src/timeline2/model/trackmodel.cpp
    ../src/timeline2/model/snapmodel.cpp
    ../src/timeline2/model/undohelper.cpp
)

include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src
    ${MLT_INCLUDE_DIR}
    ${MLTPP_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/external
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ../src
    )
ADD_EXECUTABLE(runTests ${Tests_SRCS})
target_link_libraries(runTests Qt5::Core Qt5::Widgets Qt5::Xml)
target_link_libraries(runTests
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES}
    ${CMAKE_DL_LIBS}
    KF5::I18n
    ${CMAKE_THREAD_LIBS_INIT}
    )

ADD_TEST(runTests runTests -d yes)

