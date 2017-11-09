# Pass this file to the first invocation of cmake using -DCMAKE_TOOLCHAIN_FILE=
set(CMAKE_SYSTEM_NAME Emu1)

if (DEFINED ENV{LLVM_CILK})
    set(LLVM_CILK_HOME $ENV{LLVM_CILK})
else()
    set(LLVM_CILK_HOME "/usr/local/emu-hw/toolchain")
endif()

set(CMAKE_C_COMPILER "${LLVM_CILK_HOME}/emu-cc-hw.sh")
set(CMAKE_CXX_COMPILER "${LLVM_CILK_HOME}/emu-cc-hw.sh")

set(MEMWEB_INSTALL "${LLVM_CILK_HOME}/memoryweb-libraries/memoryweb/install")
set(LIBC_INSTALL "${LLVM_CILK_HOME}/memoryweb-libraries/musl/install")

# Main CMakeLists.txt should include the following lines:

# link_directories("${LIBC_INSTALL}/lib" "${MEMWEB_INSTALL}/lib")
# link_libraries(memoryweb muslc)
