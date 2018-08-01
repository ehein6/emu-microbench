# Emu C Utilities

## Overview

Since each thread on Emu is much less powerful than a traditional x86 processor thread,
parallelism on Emu is essential to achieving good performance. This library aims to reduce
the amount of boilerplate code that must be written to implement common parallel operations.

## Workarounds
The Emu toolchain lacks support for advanced features and only targets the C language.

- **No cilk_for**: `cilk_for` is not working correctly on Emu hardware right now. It is also implemented
rather inefficiently in the simulator.
- **No lambdas**:  C doesn't have lambdas,
so instead we use a function pointer and a va_list to forward the captured data. The lambda function
needs to be defined before the function that runs a parallel loop.
Also, to avoid the overhead of an indirect function call for each element, each user-provided lambda
is expected to handle a range of elements (from `begin` to `end`) rather than a single element.
- **No operator overloading**: Indexing into an `emu_chunked_array` looks like
`emu_chunked_array_index(array, i)` instead of `array[i]`,

## Features

### emu_for_local.h

Applies a function to a range in parallel. These loops can be replaced with `cilk_for`
once it is working.

Arguments:
- `begin` - beginning of the iteration space (usually 0)
- `end` - end of the iteration space (usually array length)
- `grain` - Minimum number of elements to assign to each thread.
- `worker` - worker function that will be called on each array slice in parallel.
The loop within the worker function should go from `begin` to `end` with a stride
of `1`.
- `args` - Additional arguments to pass to each invocation of the
worker function. Arguments will be passed via the varargs interface, and you will need to cast
back to the appropriate type within the worker function using the `va_arg` macro.

Example:

```
long n = 1024;
long b = 5;
long * x = malloc(n * sizeof(long));

void worker(long begin, long end, va_list args)
{
    long * x = va_arg(args, long*);
    long b = va_arg(args, long);
    for (long i = begin; i < end; ++i) {
        x[i] += b;
    }
}
emu_local_for(0, n, LOCAL_GRAIN(n), worker, x, b);
```

## emu_for_1d.h

Implements a distributed parallel for over a `malloc1dlong` array.

Arguments:
- `array` - Pointer to striped array allocated with `malloc1dlong`.
- `size` - Length of the array
- `grain` - Minimum number of elements to assign to each thread.
- `worker` - worker function that will be called on each array slice in parallel.
The loop within the worker function should go from `begin` to `end` and have a
stride of `NODELETS()`. Each worker function will be assigned elements on a
single nodelet.
- `args` - Additional arguments to pass to each invocation of the
worker function. Arguments will be passed via the varargs interface, and you will need to cast
back to the appropriate type within the worker function using the `va_arg` macro.

```
long n = 1024;
long b = 5;
long * x = malloc1dlong(n);

void worker(long * array, long begin, long end, va_list args)
{
    long * x = array;
    long b = va_arg(args, long);
    long nodelets = NODELETS();
    for (long i = begin; i < end; i += nodelets) {
        x[i] += b;
    }
}
emu_1d_array_apply(x, n, GLOBAL_GRAIN(n), worker, b);
```

### emu_reduce_1d.h

Implements a distributed parallel reduce over a `malloc1dlong` array.

Arguments:
- `array` - Pointer to striped array allocated with `malloc1dlong`.
- `size` - Length of the array
- `grain` - Minimum number of elements to assign to each thread.
- `worker` - worker function that will be called on each array slice in parallel.
The loop within the worker function should go from `begin` to `end` and have a
stride of `NODELETS()`. Each worker function will be assigned elements on a
single nodelet, and should REMOTE_ADD to the `sum` argument.
- `args` - Additional arguments to pass to each invocation of the
worker function. Arguments will be passed via the varargs interface, and you will need to cast
back to the appropriate type within the worker function using the `va_arg` macro.

Example:

```
long n = 1024;
long b = 5;
long * x = malloc1dlong(n);

void worker(long * array, long begin, long end, long * sum, void * arg1)
{
    long * x = array;
    long b = (long)arg1;
    long nodelets = NODELETS();
    long local_sum = 0;
    for (long i = begin; i < end; i += nodelets) {
        local_sum += x[i] * b;
    }
    REMOTE_ADD(sum, local_sum);
}
long sum = emu_1d_array_reduce_sum_v2(x, n, GLOBAL_GRAIN(n), worker, (void*)b);
```

### emu_chunked_array.h

Implements a datatype for a distributed array with elements with arbitrary size.
Internally, a `mw_malloc2D` array with
blocked allocation places consecutive elements on the same nodelet.

`emu_chunked_array_replicated_new` allocates and initializes a `emu_chunked_array`,
returning a replicated pointer to the data structure. The array
 is striped across all nodelets, and the metadata is placed in replicated storage.
`emu_chunked_array_replicated_free` should be used to free the array.

An `emu_chunked_array` can also be placed in global replicated storage, or nested within
another struct within replicated memory. In these cases use `emu_chunked_array_replicated_init`
to initialize and `emu_chunked_array_replicated_deinit` to de-initialize the array.

`emu_chunked_array_index` returns a pointer to the `i`th element within the array.
Remember to cast the returned value to the appropriate type before dereferencing.

`emu_chunked_array_size` returns the number of elements in the array.

### emu_for_2d.h

Implements a distributed parallel for over `emu_chunked_array` types.

Arguments:
- `array` - Pointer to previously initialized chunked array struct.
- `grain` - Minimum number of elements to assign to each thread.
- `worker` - worker function that will be called on each array slice in parallel.
The loop within the worker function is responsible for array elements from `begin` to `end`
with a stride of `1`. Because each worker function will be assigned elements on a
single nodelet, it is more efficient to call `emu_chunked_array_index` once before the loop,
and do linear indexing from that pointer, as shown.
- `args` - Additional arguments to pass to each invocation of the
worker function. Arguments will be passed via the varargs interface, and you will need to cast
back to the appropriate type within the worker function using the `va_arg` macro.

Example:

```
long n = 1024;
long b = 5;
emu_chunked_array * x = emu_chunked_array_replicated_new(n, sizeof(long));

void
worker(emu_chunked_array * array, long begin, long end, va_list args)
{
    long b = va_arg(args, long);
    long * x = emu_chunked_array_index(array, begin);

    for (long i = 0; i < end-begin; ++i) {
        x[i] += b;
    }
}
emu_chunked_array_apply(x, GLOBAL_GRAIN(n), worker, b);
```

### emu_reduce_2d.h

`emu_chunked_array_reduce` works similarly to `emu_1d_array_reduce`.

### memoryweb_x86.h

This header aims to implement all the intrinsics in `memoryweb.h` for the x86 architecture.
This way you can compile and test your code quickly in a native environment before moving to
the Emu simulator or hardware.

### hooks.h

Provides a simple set of performance counters.
Surround the region of interest with `hooks_region_begin` and `hooks_region_end`.
After the region of interest is complete, the time elapsed will be printed to stdout
(can be changed by setting HOOKS_FILENAME in your environment) in JSON format.
Additional keys can be added to the JSON output by calling a version of `hooks_set_attr`
before the beginning of the region.

Region markers will also call `starttiming()` to enable detailed timing mode in the
Emu simulator. If there are multiple regions with distinct names, then
`hooks_set_active_region` can be called to make only one region trigger timing mode.
You may wish to set this via an environment variable in your own code.

IMPORTANT: Currently there is no way for software to detect the system clock rate. Set
the environment variable `CORE_CLK_MHZ` to 150 or 300 as appropriate. Otherwise the
time in the output will be inaccurate. The number of raw clock ticks is also included
in the output for convenience.

### emu_grain_helpers.h

The parallel apply functions accept a grain size argument. In order to avoid
overwhelming the system with too many threads, it is usually best to spawn
just enough threads to avoid overwhelming the system. Assuming `GCS_PER_NODELET`
is set correctly in your environment, `LOCAL_GRAIN()` will calculate a grain size
from the array length to spawn exactly enough threads to saturate a single nodelet.
`GLOBAL_GRAIN()` will do the same for the entire system. `GLOBAL_GRAIN_MIN` and
`LOCAL_GRAIN_MIN()` accept an additional grain size argument to avoid spawning
too many threads for a small array.

### emu_sort_local.h

Initial implementation of local parallel sort contributed by Anirudh Jain <anirudh.j@gatech.edu>.

### emu_scatter_gather.h

Functions to convert between to/from a nodelet-local array and a chunked array in parallel.

### layout.h

Helper functions for examining an Emu pointer. `examine_emu_pointer` will return a struct that
breaks the pointer into bit fields, and `print_emu_pointer` will print the pointer in human-readable
format. `pointers_on_same_nodelet` will determine whether or not two absolute pointers refer to
storage on the same nodelet. This function is not well optimized and should be used for debugging
only.
