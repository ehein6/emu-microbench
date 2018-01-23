cmake_minimum_required (VERSION 3.5.1)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(emu-microbench NONE)
enable_language(C)

# Use the C11 standard
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
list(APPEND ${CMAKE_CXX_FLAGS} "-std=c++11")

# Turn warnings into errors
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

if (CMAKE_SYSTEM_NAME STREQUAL "Emu1")
    set(BUILD_FOR_EMUSIM OFF
        CACHE BOOL "If enabled, will call starttiming() at the beginning of the ROI")

    if (${BUILD_FOR_EMUSIM})
        add_definitions(-DBUILD_FOR_EMUSIM)
    endif()

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    link_directories("${LIBC_INSTALL}/lib" "${MEMWEB_INSTALL}/lib")
    link_libraries(memoryweb muslc)
else()
    # Enable Cilk
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcilkplus")
    link_libraries(cilkrts)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
endif()

add_subdirectory(emu_c_utils)
include_directories(emu_c_utils)
link_libraries(emu_c_utils)

add_executable(local_stream local_stream.c)
add_executable(global_stream global_stream.c)
add_executable(global_reduce global_reduce.c)
add_executable(pointer_chase pointer_chase.c)
