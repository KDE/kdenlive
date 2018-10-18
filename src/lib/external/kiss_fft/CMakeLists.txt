# Create a static library from kiss_fft
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")
find_package(Qt5Core REQUIRED)
include_directories(
  ${CMAKE_BINARY_DIR}
)
add_library(kiss_fft STATIC
  _kiss_fft_guts.h
  kiss_fft.c
  tools/kiss_fftr.c
)
target_link_libraries(kiss_fft Qt5::Core)
