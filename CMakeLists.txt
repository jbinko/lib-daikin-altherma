add_library(
    libdaikin
    include/libdaikin.h
    include/libdaikinhal.h
    src/libdaikin.cpp
    src/websockets.cpp
    src/websockets_frame.cpp
    )

target_include_directories(
    libdaikin
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
