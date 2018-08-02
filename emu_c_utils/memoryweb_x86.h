/*! \file memoryweb_x86.h
 \date March 15, 2018
 \author Eric Hein 
 \brief Implements the Emu intrinsics and memoryweb library functions for x86 architectures.
 Include this file instead of <memoryweb.h> to compile and test Emu programs natively

 */

#ifndef _MEMORYWEB_H
#define _MEMORYWEB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>

/* Data Allocation and Distribution */

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

static inline void *
mw_malloc2d(size_t nelem, size_t sz)
{
    // We need an 8-byte pointer for each element, plus the array of elements
    size_t bytes = nelem * sizeof(long) + nelem * sz;
    unsigned char ** ptrs = (unsigned char **)malloc(bytes);
    if (ptrs == NULL) return NULL;
    // Skip past the pointers to get to the raw array
    unsigned char * data = (unsigned char *)ptrs + nelem * sizeof(long);
    // Assign pointer to each element
    for (size_t i = 0; i < nelem; ++i) {
        ptrs[i] = data + i * sz;
    }
    return ptrs;
}

static inline void
mw_free(void * ptr)
{
    free(ptr);
}

static inline void
mw_localfree(void * localpointer)
{
    free(localpointer);
}

static inline void *
mw_arrayindex(long * array2d, unsigned long i, unsigned long numelements, size_t eltsize)
{
    unsigned char ** array = (unsigned char**) array2d;
    return &array[i][0];
}

#define replicated

static inline void
mw_replicated_init(long * repl_addr, long value)
{
    *repl_addr = value;
}

static inline void
mw_replicated_init_multiple(long * repl_addr, long (*init_func)(long))
{
    *repl_addr = init_func(0);
}

static inline void
mw_replicated_init_generic(void * repl_addr, void (*init_func)(void *, long))
{
    init_func(repl_addr, 0);
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

/* Architecture Specific Operations */

static inline long
ATOMIC_CAS(volatile long * ptr, long newval, long oldval) {
    return __sync_val_compare_and_swap(ptr, oldval, newval);
}

static inline long
ATOMIC_SWAP(volatile long * ptr, long newval) {
    long oldval;
    do { oldval = *ptr; } while (oldval != __sync_val_compare_and_swap(ptr, oldval, newval));
    return oldval;
}

// No suffix variants: do the op without modifying memory
static inline long ATOMIC_ADD(volatile long * ptr, long val) { return *ptr + val; }
static inline long ATOMIC_AND(volatile long * ptr, long val) { return *ptr & val; }
static inline long ATOMIC_OR (volatile long * ptr, long val) { return *ptr | val; }
static inline long ATOMIC_XOR(volatile long * ptr, long val) { return *ptr ^ val; }
static inline long ATOMIC_MAX(volatile long * ptr, long val) {
    long x = *ptr;
    return val > x ? val : x;
}
static inline long ATOMIC_MIN(volatile long * ptr, long val) {
    long x = *ptr;
    return val < x ? val : x;
}

// M-suffix variants: write result to memory and return result
#define ATOMIC_ADDM(PTR, VAL) __sync_add_and_fetch(PTR, VAL)
#define ATOMIC_ANDM(PTR, VAL) __sync_and_and_fetch(PTR, VAL)
#define ATOMIC_ORM(PTR, VAL)  __sync_or_and_fetch(PTR, VAL)
#define ATOMIC_XORM(PTR, VAL) __sync_xor_and_fetch(PTR, VAL)
static inline long
ATOMIC_MAXM(volatile long * ptr, long value) {
    long x;
    do {
        x = *ptr;
        if (x >= value) return x;
    } while (__sync_bool_compare_and_swap(ptr, x, value) == false);
    return value;
}

static inline long
ATOMIC_MINM(volatile long * ptr, long value) {
    long x;
    do {
        x = *ptr;
        if (x <= value) return x;
    } while (__sync_bool_compare_and_swap(ptr, x, value) == false);
    return value;
}

// S-suffix variants: write val to memory and return result
static inline long
ATOMIC_ADDS(volatile long * ptr, long val) {
    long x = ATOMIC_SWAP(ptr, val);
    return x + val;
}

static inline long
ATOMIC_ANDS(volatile long * ptr, long val) {
    long x = ATOMIC_SWAP(ptr, val);
    return x & val;
}

static inline long
ATOMIC_ORS(volatile long * ptr, long val) {
    long x = ATOMIC_SWAP(ptr, val);
    return x | val;
}

static inline long
ATOMIC_XORS(volatile long * ptr, long val) {
    long x = ATOMIC_SWAP(ptr, val);
    return x ^ val;
}

static inline long
ATOMIC_MAXS(volatile long * ptr, long val) {
    long x = ATOMIC_SWAP(ptr, val);
    return val > x ? val : x;
}

static inline long
ATOMIC_MINS(volatile long * ptr, long val) {
    long x = ATOMIC_SWAP(ptr, val);
    return val < x ? val : x;
}

// MS-suffix variants: write result to memory and return old value
#define ATOMIC_ADDMS(PTR, VAL) __sync_fetch_and_add(PTR, VAL)
#define ATOMIC_ANDMS(PTR, VAL) __sync_fetch_and_and(PTR, VAL)
#define ATOMIC_ORMS(PTR, VAL) __sync_fetch_and_or(PTR, VAL)
#define ATOMIC_XORMS(PTR, VAL) __sync_fetch_and_xor(PTR, VAL)

static inline long
ATOMIC_MAXMS(volatile long * ptr, long value) {
    long x;
    do {
        x = *ptr;
        if (x >= value) break;
    } while (__sync_bool_compare_and_swap(ptr, x, value) == false);
    return x;
}

static inline long
ATOMIC_MINMS(volatile long * ptr, long value) {
    long x;
    do {
        x = *ptr;
        if (x <= value) break;
    } while (__sync_bool_compare_and_swap(ptr, x, value) == false);
    return x;
}

static inline void
REMOTE_ADD(volatile long * ptr, long value) { ATOMIC_ADDMS(ptr, value); }
static inline void
REMOTE_AND(volatile long * ptr, long value) { ATOMIC_ANDMS(ptr, value); }
static inline void
REMOTE_OR(volatile long * ptr, long value) { ATOMIC_ORMS(ptr, value); }
static inline void
REMOTE_XOR(volatile long * ptr, long value) { ATOMIC_XORMS(ptr, value); }
static inline void
REMOTE_MAX(volatile long * ptr, long value) { ATOMIC_MAXMS(ptr, value); }
static inline void
REMOTE_MIN(volatile long * ptr, long value) { ATOMIC_MINMS(ptr, value); }

#define FENCE()

static inline long
POPCNT(long sum, long* val)
{
    return sum + __builtin_popcountl(*val);
}

static unsigned long
PRIORITY(unsigned long val)
{
    return 63 - __builtin_clzl(val);
}

#define RESIZE()
#define RESCHEDULE()

#include <sys/time.h>
#define MEMORYWEB_X86_CLOCK_RATE (500L)
static inline long CLOCK()
{
    struct timeval tp;
    gettimeofday(&tp,NULL);
    double time_seconds = ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
    return (long)(time_seconds * (MEMORYWEB_X86_CLOCK_RATE * 1e6));
}

#define MAXDEPTH() (255L)
#define THREAD_ID() (__cilkrts_get_worker_number())
#define NODE_ID() (0L)
#define NODELETS() (1L)
#define BYTES_PER_NODELET() (8589934592L) // 8GB
#define MIGRATE(X) ((void)X)
#define noinline
#define starttiming()
#define stoptiming()

#ifdef __cplusplus
}
#endif

#endif