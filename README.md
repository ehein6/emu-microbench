# Overview

Microbenchmarks to test the performance characteristics of the Emu Chick.

# Building

Build for emu hardware:
```
mkdir build-hw && cd build-hw
cmake .. \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_TOOLCHAIN_FILE=../cmake/emu-17.11-hw.toolchain.cmake \
-DBUILD_FOR_EMUSIM=OFF
make -j4
```

Build for emu simulator:
```
mkdir build-sim && cd build-sim
cmake .. \
-DCMAKE_BUILD_TYPE=Release \
-DCMAKE_TOOLCHAIN_FILE=../cmake/emu-17.11-hw.toolchain.cmake \
-DBUILD_FOR_EMUSIM=ON
make -j4
```

Build for testing on x86:
```
mkdir build-x86 && cd build-x86
cmake .. \
-DCMAKE_BUILD_TYPE=Debug
make -j4
```

# Benchmarks

## `local_stream`

### Description
Allocates three arrays (A, B, C) with 2^`log2_num_elements` on a single nodelet. Computes the sum of two vectors (C = A + B) with `num_threads` threads, and reports the average memory bandwidth.

### Usage

`./local_stream mode log2_num_elements num_threads`

### Modes

- serial - Uses a serial for loop
- cilk_for - Uses a cilk_for loop
- serial_spawn - Uses a serial for loop to spawn a thread for each grain-sized chunk of the loop range
- recursive_spawn - Recursively spawns threads to divide up the loop range

## `global_stream`
Allocates three arrays (A, B, C) with 2^`log2_num_elements` using a 2D array distributed across all the nodelets. Computes the sum of two vectors (C = A + B) with `num_threads` threads, and reports the average memory bandwidth.

### Usage

`./global_stream mode log2_num_elements num_threads`

### Modes

- serial - Uses a serial for loop
- cilk_for - Uses a cilk_for loop
- serial_spawn - Uses a serial for loop to spawn a thread for each grain-sized chunk of the loop range
- recursive_spawn - Recursively spawns threads to divide up the loop range
- recursive_remote_spawn (not implemented) - Recursively spawns threads to divice up the loop range, using remote spawns where possible.
- serial_remote_spawn - Remote spawns a thread on each nodelet, then divides up work as in serial_spawn
- serial_remote_spawn_shallow - Like serial_remote_spawn, but all threads are remote spawned from nodelet 0.

