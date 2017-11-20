cmake_minimum_required (VERSION 3.5.1)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(emu-microbench NONE)
enable_language(C)

# Use the C11 standard
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
# Turn warnings into errors
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
# Enable Cilk
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcilkplus")

# Disable some errant warnings caused by cilk_for
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-uninitialized -Wno-cilk-loop-control-var-modification")

# Arch-specific directives
if (CMAKE_SYSTEM_NAME STREQUAL "Emu1")
    # Add flag to build for simulator or hardware
    set(BUILD_FOR_EMUSIM OFF
        CACHE BOOL "If enabled, will call starttiming() at the beginning of the ROI")
    if (${BUILD_FOR_EMUSIM})
        add_definitions(-DBUILD_FOR_EMUSIM)
    endif()

    # Link with memoryweb and muslc
    link_directories("${LIBC_INSTALL}/lib" "${MEMWEB_INSTALL}/lib")
    link_libraries(memoryweb muslc)
else()
    # Link with cilk runtime
    link_libraries(cilkrts)
endif()

# Build benchmarks
add_executable(local_stream local_stream.c timer.c timer.h recursive_spawn.h)
add_executable(global_stream global_stream.c timer.c timer.h recursive_spawn.h)

set(ENABLE_CXX FALSE
    CACHE BOOL "Build C++ benchmarks")

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emu1")
    set(ENABLE_CXX TRUE)
endif()

# If C++ supported, build C++ benchmarks also
if (${ENABLE_CXX})
    enable_language(CXX)
    # Use the C++11 standard
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    # Turn warnings into errors
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
    # Enable Cilk
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcilkplus")
    # Disable some errant warnings caused by cilk_for
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-uninitialized")

    # Include support libraries
    include_directories(emu_cxx_utils)

    # Build C++ benchmarks
    add_executable(local_stream_cxx
        local_stream.cc
        timer.c timer.h
    )
    add_executable(global_stream_cxx
        global_stream.cc
        timer.c timer.h
    )
endif()
