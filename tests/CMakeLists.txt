if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")
endif()
set(Tests_SRCS
    TestMain.cpp
    abortutil.cpp
    compositiontest.cpp
    effectstest.cpp
    groupstest.cpp
    keyframetest.cpp
    markertest.cpp
    modeltest.cpp
    regressions.cpp
    snaptest.cpp
    test_utils.cpp
    timewarptest.cpp
    treetest.cpp
    trimmingtest.cpp
)
include_directories(${MLT_INCLUDE_DIR} ..)
add_executable(runTests ${Tests_SRCS})
set_property(TARGET runTests PROPERTY CXX_STANDARD 14)
target_link_libraries(runTests kdenliveLib)
add_test(NAME runTests COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/runTests -d yes)
