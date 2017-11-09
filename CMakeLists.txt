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
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    link_directories("${LIBC_INSTALL}/lib" "${MEMWEB_INSTALL}/lib")
    link_libraries(memoryweb muslc)
else()
    # Enable Cilk
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcilkplus")
    link_libraries(cilkrts)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
endif()


add_executable(local_stream local_stream.c timer.c)