#ifndef _MEMORYWEB_H
#define _MEMORYWEB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

// Mimic memoryweb behavior on x86
// TODO eventually move this all to its own header file
#define NODE_ID() (0)
#define NODELETS() (1)
#define replicated
#define PRIORITY(X) (63-__builtin_clzll(X))
#define MIGRATE(X) ((void)X)
// For emu, we declare spawned functions noinline to prevent variables from the parent
// stack frame from being carried along. For x86, inlining is good.
#define noinline

#include <sys/time.h>
#define CLOCK_RATE (1e6)
static inline long CLOCK()
{
    struct timeval tp;
    gettimeofday(&tp,NULL);
    double time_seconds = ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
    return time_seconds * CLOCK_RATE;
}

static inline void *
mw_get_nth(void * repl_addr, long n) {
    (void)n;
    return repl_addr;
}

static inline void *
mw_get_localto(void * repl_addr, void * localpointer) {
    (void)localpointer;
    return repl_addr;
}

static inline void *
mw_mallocrepl(size_t sz)
{
    return malloc(sz);
}

static inline void
mw_replicated_init(long * repl_addr, long value)
{
    *repl_addr = value;
}

static inline void *
mw_malloc2d(size_t nelem, size_t sz)
{
    // We need an 8-byte pointer for each element, plus the array of elements
    size_t bytes = nelem * sizeof(long) + nelem * sz;
    unsigned char ** ptrs = (unsigned char **)malloc(bytes);
    // Skip past the pointers to get to the raw array
    unsigned char * data = (unsigned char *)ptrs + nelem * sizeof(long);
    // Assign pointer to each element
    for (size_t i = 0; i < nelem; ++i) {
        ptrs[i] = data + i * sz;
    }
    return ptrs;
}

static inline void *
mw_localmalloc(size_t sz, void * localpointer)
{
    (void)localpointer;
    return malloc(sz);
}

static inline void *
mw_malloc1dlong(size_t nelem)
{
    return malloc(nelem * sizeof(long));
}

static inline void
mw_free(void * ptr)
{
    free(ptr);
}

#define ATOMIC_ADDMS(PTR, VAL) __sync_fetch_and_add(PTR, VAL)

static inline long
ATOMIC_CAS(volatile long * ptr, long newval, long oldval) {
    return __sync_val_compare_and_swap(ptr, oldval, newval);
}

static inline long
ATOMIC_MAXMS(volatile long * ptr, long value) {
    long x;
    do {
        x = *ptr;
        if (x >= value) return x;
    } while (__sync_bool_compare_and_swap(ptr, x, value) == false);
    return value;
}

static inline long
ATOMIC_MINMS(volatile long * ptr, long value) {
    long x;
    do {
        x = *ptr;
        if (x <= value) return x;
    } while (__sync_bool_compare_and_swap(ptr, x, value) == false);
    return value;
}

static inline void
REMOTE_ADD(volatile long * ptr, long value) { ATOMIC_ADDMS(ptr, value); }
static inline void
REMOTE_MAX(volatile long * ptr, long value) { ATOMIC_MAXMS(ptr, value); }
static inline void
REMOTE_MIN(volatile long * ptr, long value) { ATOMIC_MINMS(ptr, value); }

#ifdef __cplusplus
}
#endif

#endif // _MEMORYWEB_H