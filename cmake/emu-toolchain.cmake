# Pass this file to the first invocation of cmake using -DCMAKE_TOOLCHAIN_FILE=
set(CMAKE_SYSTEM_NAME Emu1)

if (DEFINED ENV{LLVM_CILK})
    set(LLVM_CILK_HOME $ENV{LLVM_CILK})
else()
    set(LLVM_CILK_HOME "/usr/local/emu")
endif()

set(CMAKE_C_COMPILER "${LLVM_CILK_HOME}/bin/emu-cc.sh")
set(CMAKE_CXX_COMPILER "${LLVM_CILK_HOME}/bin/emu-cc.sh")

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH ${LLVM_CILK_HOME}/gossamer64)

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
# search for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(MEMWEB_INSTALL "${LLVM_CILK_HOME}")
set(LIBC_INSTALL "${LLVM_CILK_HOME}")

# Main CMakeLists.txt should include the following lines:

# link_directories("${LIBC_INSTALL}/lib" "${MEMWEB_INSTALL}/lib")
# link_libraries(memoryweb muslc)