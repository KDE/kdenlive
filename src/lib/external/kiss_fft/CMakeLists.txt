# Create a static library from kiss_fft
find_package(Qt${QT_MAJOR_VERSION}Core REQUIRED)
include_directories(${CMAKE_BINARY_DIR})
add_library(kiss_fft STATIC _kiss_fft_guts.h kiss_fft.c tools/kiss_fftr.c)
set_property(TARGET kiss_fft PROPERTY C_STANDARD 99)
target_link_libraries(kiss_fft Qt${QT_MAJOR_VERSION}::Core)
