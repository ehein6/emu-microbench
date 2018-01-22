# Emu C Utilities

## Overview

Since each thread on Emu is much less powerful than a traditional x86 processor thread,
parallelism on Emu is essential to achieving good performance. This library aims to reduce
the amount of boilerplate code that must be written to implement common parallel operations.

## Workarounds
The Emu toolchain lacks support for advanced features and only targets the C language.  

- **No cilk_for**: `cilk_for` is not working correctly on Emu hardware right now. It is also implemented
rather inefficiently in the simulator. 
- **No lambdas**: If I were writing this in C++, I would have each parallel function accept a lambda
function argument to indicate what operation to perform on each element. C doesn't have lambdas, 
so instead we use a function pointer and a void pointer to the captured data. The lambda function
needs to be defined before the function that runs a parallel loop, which makes the code harder to read.
Also, to avoid the overhead of an indirect function call for each element, each user-provided lambda
is expected to handle a range of elements (from `begin` to `end`) rather than a single element.
- **No varargs**: Trying to `cilk_spawn` a function with `varargs` resulted in data corruption.
Probably the size of the spawned function's stack frame needs to be known at compile-time.
I used [Cog](https://nedbatchelder.com/code/cog/) to generate several versions of each function with a 
varying number of arguments. This technique has the advantage that passed variables will be 
copied to the remote nodelet on a spawn rather than forcing a migration every time they are touched. 
The downside is that arguments need to be carefully casted to/from `void*`.
- **No operator overloading**: Indexing into an `emu_chunked_array` looks like 
`emu_chunked_array_index(array, i)` instead of `array[i]`, 

## Features

### emu_for_local

Applies a function to a range in parallel. These loops can be replaced with `cilk_for`
once it is working. 

### emu_chunked_array

Implements a datatype for a distributed array of elements with arbitrary size. 

Elements of a `mw_malloc2D` array are striped across nodelets. This is not ideal
for iteration since there will be one migration per element. `emu_chunked_array` uses a
blocked allocation so that consecutive elements are stored on the same nodelet. 

`emu_chunked_array_apply` implements a two-level spawn tree, first spawning a thread
at each nodelet, and then locally spawning threads to traverse the entire array. Within a
given invocation of the lambda function, all elements will be on the same nodelet. This 
allows regular array indexing instead of the more generic `emu_chunked_array_index` function.

`emu_chunked_array_reduce` accumulates the reduction locally before using remote writes to
aggregate across all nodelets. 

### memoryweb_x86.h

This header aims to implement all the intrinsics in `memoryweb.h` for the x86 architecture.
This way you can compile and test your code quickly in a native environment before moving to 
the Emu simulator or hardware.
 


    