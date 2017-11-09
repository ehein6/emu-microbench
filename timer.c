#include "timer.h"
#include <stdio.h>

#ifdef __le64__
#include <memoryweb.h>
#define CLOCK_RATE (154e6)
#else
#include <unistd.h>
#include <sys/time.h>
#define CLOCK_RATE (1e6)
long CLOCK()
{
    struct timeval tp;
    int i;

    i = gettimeofday(&tp,NULL);
    double time_seconds = ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
    return time_seconds * CLOCK_RATE;
}

#define MIGRATE(X)
#endif

static long timer_timestamp;

void
timer_start()
{
    MIGRATE(&timer_timestamp);
    timer_timestamp = -CLOCK();
}

long
timer_stop()
{
    MIGRATE(&timer_timestamp);
    return timer_timestamp + CLOCK();
}



double
timer_calc_bandwidth(long ticks, long bytes)
{
    if (ticks == 0) return 0;
    double time_seconds = ticks / CLOCK_RATE;
    printf("%li ticks elapsed, %3.2f seconds\n", ticks, time_seconds);
    double bytes_per_second = bytes / time_seconds;
    return bytes_per_second;
}

void
timer_print_bandwidth(const char* name, double bytes_per_second)
{
    printf("%s: %3.2f MiB/s\n", name, bytes_per_second / (1024 * 1024));
    fflush(stdout);
}
