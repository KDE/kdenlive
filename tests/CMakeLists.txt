############################
# Tests
############################

set(QT_MIN_VERSION 5.5.0)
find_package(Qt5Core)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fexceptions")


SET(Tests_SRCS
    TestMain.cpp
    modeltest.cpp
    groupstest.cpp
    ../src/timeline2/model/trackmodel.cpp
    ../src/timeline2/model/timelinemodel.cpp
    ../src/timeline2/model/clipmodel.cpp
    ../src/timeline2/model/groupsmodel.cpp
)

INCLUDE_DIRECTORIES(../src/)
include_directories(
    ${CMAKE_BINARY_DIR}
    ${MLT_INCLUDE_DIR}
    ${MLTPP_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/external
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    )
ADD_EXECUTABLE(runTests ${Tests_SRCS})
target_link_libraries(runTests Qt5::Core)
target_link_libraries(runTests 
    ${MLT_LIBRARIES}
    ${MLTPP_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    )

ADD_TEST(runTests runTests -d yes)

