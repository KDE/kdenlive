include_directories(${MLT_INCLUDE_DIR} ${MLTPP_INCLUDE_DIR} ..)
set(TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR})
configure_file(tests_definitions.h.in tests_definitions.h)
kde_enable_exceptions()

set(KdenliveTest_SOURCES
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
    timewarptest.cpp
    titlertest.cpp
    treetest.cpp
    trimmingtest.cpp
    cachetest.cpp
    movetest.cpp
    subtitlestest.cpp
)

include(ECMAddTests)

foreach(_source ${KdenliveTest_SOURCES})
  get_filename_component(_targetname ${_source} NAME_WE)
  ecm_add_test(
      TestMain.cpp
      test_utils.cpp
      abortutil.cpp
      ${_source}
      TEST_NAME ${_targetname}
      LINK_LIBRARIES kdenliveLib
  )
  set_property(TARGET ${_targetname} PROPERTY CXX_STANDARD 14)
endforeach()
