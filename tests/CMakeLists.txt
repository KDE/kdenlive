############################
# Tests
############################
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -fexceptions")


SET(Tests_SRCS
    tests/TestMain.cpp
    tests/compositiontest.cpp
    tests/groupstest.cpp
    tests/markertest.cpp
    tests/modeltest.cpp
    tests/regressions.cpp
    tests/snaptest.cpp
    tests/treetest.cpp
    PARENT_SCOPE
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

