############################
# fuzzing
########################

if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()

project(Kdenlive_fuzzing)

SET(FUZZING_CXX_FLAGS "${DEFAULT_CXX_FLAGS} -Wall -pedantic -Wextra -fexceptions -O1  -fsanitize=fuzzer,address -g")



SET(fuzzing_SRCS
  main_fuzzer.cpp
  fuzzing.cpp
)
SET(reproduce_SRCS
  main_reproducer.cpp
  fuzzing.cpp
)

ADD_EXECUTABLE(fuzz ${fuzzing_SRCS})
ADD_EXECUTABLE(fuzz_reproduce ${reproduce_SRCS})
target_link_libraries(fuzz kdenliveLib)
target_link_libraries(fuzz_reproduce kdenliveLib)
#target_link_options(fuzz PUBLIC "-fsanitize=fuzzer")
set_target_properties(fuzz PROPERTIES LINK_FLAGS "-fsanitize=fuzzer")
set_property(TARGET fuzz PROPERTY CXX_STANDARD 14)
set_property(TARGET fuzz_reproduce PROPERTY CXX_STANDARD 14)
set_target_properties(fuzz PROPERTIES COMPILE_FLAGS "${FUZZING_CXX_FLAGS}")
