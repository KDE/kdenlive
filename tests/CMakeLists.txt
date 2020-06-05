include_directories(${MLT_INCLUDE_DIR} ..)
kde_enable_exceptions()
add_executable(runTests
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
set_property(TARGET runTests PROPERTY CXX_STANDARD 14)
target_link_libraries(runTests kdenliveLib)
add_test(NAME runTests COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/runTests -d yes)
