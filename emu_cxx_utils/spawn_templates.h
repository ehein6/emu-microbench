#pragma once

#include <cilk/cilk.h>

namespace emu {

template<typename F>
void
local_serial_spawn(long low, long high, long grain, F worker)
{
    if (high - low > grain) {
        // Iterate through the range one grain at a time
        for (long i = low; i < high; i += grain) {
            long begin = i;
            // The last chunk might not be a full grain if it doesn't divide evenly
            long end = begin + grain <= high ? begin + grain : high;
            // Spawn a thread to deal with this chunk
            cilk_spawn local_serial_spawn(begin, end, grain, worker);
        }
    } else {
        for (long j = low; j < high; ++j) {
            worker(j);
        }
    }
}

template<typename F>
void
local_recursive_spawn(long low, long high, long grain, F worker)
{
    for (;;) {
        /* How many elements in my range? */
        long count = high - low;

        /* Break out when my range is smaller than the grain size */
        if (count <= grain) break;

        /* Divide the range in half */
        /* Invariant: count >= 2 */
        long mid = low + count / 2;

        /* Spawn a thread to deal with the lower half */
        cilk_spawn local_recursive_spawn(low, mid, grain, worker);

        low = mid;
    }

    /* Recursive base case: call worker function */
    for (long i = low; i < high; ++i){
        worker(i);
    }
}

template <typename F>
void
local_spawn(long low, long high, F worker, long grain = 64)
{
    local_serial_spawn(low, high, grain, worker);
}

} // end namespace emu