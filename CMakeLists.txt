cmake_minimum_required (VERSION 3.5.1)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(emu-microbench C CXX)

# Use the 2011 standards for C and C++
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
# Turn warnings into errors
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
# Enable Cilk
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcilkplus")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcilkplus")

# Disable some errant warnings caused by cilk_for
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused -Wno-uninitialized")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-uninitialized")

# Arch-specific directives
if (CMAKE_SYSTEM_NAME STREQUAL "Emu1")
    # Disable some errant warnings caused by cilk_for
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-cilk-loop-control-var-modification")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cilk-loop-control-var-modification")
else()
    # Link with cilk runtime
    link_libraries(cilkrts)
endif()

add_subdirectory(emu_c_utils)
include_directories(emu_c_utils)
link_libraries(emu_c_utils)

set(ENABLE_VALIDATION "ON"
    CACHE BOOL "Validate results after each benchmark. The initialization and validation steps may spawn more
                threads than were requested on the command line."
)
if (NOT ENABLE_VALIDATION)
    add_definitions("-DNO_VALIDATE")
endif()

add_executable(local_stream local_stream.c)
add_executable(global_stream global_stream.c)
add_executable(global_stream_1d global_stream_1d.c)
# add_executable(global_reduce global_reduce.c)
# add_executable(pointer_chase pointer_chase.c)
# add_executable(ping_pong ping_pong.c)
# add_executable(local_sort local_sort.c)
# add_executable(bulk_copy bulk_copy.c)
# add_executable(scatter scatter.c)

# C++ benchmarks
# Include support libraries
# include_directories(emu_cxx_utils)
# add_executable(local_stream_cxx local_stream_cxx.cc)
# add_executable(global_stream_cxx global_stream_cxx.cc)
