# Pass this file to the first invocation of cmake using -DCMAKE_TOOLCHAIN_FILE=
set(CMAKE_SYSTEM_NAME Emu1)

if (DEFINED ENV{LLVM_CILK})
    set(LLVM_CILK_HOME $ENV{LLVM_CILK})
else()
    set(LLVM_CILK_HOME "/usr/local/emu")
endif()

set(CMAKE_C_COMPILER "${LLVM_CILK_HOME}/bin/emu-cc.sh")
set(CMAKE_CXX_COMPILER "${LLVM_CILK_HOME}/bin/emu-cc.sh")
set(CMAKE_FIND_ROOT_PATH ${LLVM_CILK_HOME}/gossamer64)
