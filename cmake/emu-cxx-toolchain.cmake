# Pass this file to the first invocation of cmake using -DCMAKE_TOOLCHAIN_FILE=
set(CMAKE_SYSTEM_NAME Emu1)

set(LLVM_CILK_HOME "/usr/local/emu-17.08.1/")

set(CMAKE_C_COMPILER "${LLVM_CILK_HOME}/bin/emu-cc.sh")
set(CMAKE_CXX_COMPILER "${LLVM_CILK_HOME}/bin/emu-cc.sh")

set(MEMWEB_INSTALL "${LLVM_CILK_HOME}/lib")
set(LIBC_INSTALL "${LLVM_CILK_HOME}/lib")

# Main CMakeLists.txt should include the following lines:

# link_directories("${LIBC_INSTALL}/lib" "${MEMWEB_INSTALL}/lib")
# link_libraries(memoryweb muslc)
