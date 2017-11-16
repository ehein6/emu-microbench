template<typename F>
void
local_serial_spawn(long low, long high, long grain, F worker)
{
    for (long i = low; i < high; i += grain) {
        long begin = i;
        long end = begin + grain <= high ? begin + grain : high;
        cilk_spawn [=](){ for (long i = begin; i < end; ++i) { worker(i); }}();
    }
    cilk_sync;
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