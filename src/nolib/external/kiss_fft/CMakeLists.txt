set(kdenlive_SRCS
  ${kdenlive_SRCS}
  nolib/external/kiss_fft/_kiss_fft_guts.h
  nolib/external/kiss_fft/kiss_fft.c
  nolib/external/kiss_fft/tools/kiss_fftr.c
  PARENT_SCOPE
)


# Create a static library from kiss_fft

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c99")
include_directories(
  ${CMAKE_BINARY_DIR}
)
add_library(kiss_fft STATIC
  _kiss_fft_guts.h
  kiss_fft.c
  tools/kiss_fftr.c
)
