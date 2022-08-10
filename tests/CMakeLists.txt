include_directories(${MLT_INCLUDE_DIR} ${MLTPP_INCLUDE_DIR} ..)
set(TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR})
configure_file(tests_definitions.h.in tests_definitions.h)
kde_enable_exceptions()
add_executable(runTests
    TestMain.cpp
    abortutil.cpp
    colorscopestest.cpp
    compositiontest.cpp
    effectstest.cpp
    filetest.cpp
    mixtest.cpp
    groupstest.cpp
    keyframetest.cpp
    markertest.cpp
    modeltest.cpp
    regressions.cpp
    snaptest.cpp
    test_utils.cpp
    timewarptest.cpp
    titlertest.cpp
    treetest.cpp
    trimmingtest.cpp
    cachetest.cpp
    movetest.cpp
    #subtitlestest.cpp
)
set_property(TARGET runTests PROPERTY CXX_STANDARD 14)
target_link_libraries(runTests kdenliveLib)
add_test(NAME runTests COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/runTests -d yes)
