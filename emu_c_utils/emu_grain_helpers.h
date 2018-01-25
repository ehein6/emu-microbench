#pragma once

// TODO get max number of threads on platform
#ifdef __le64__
// Emu Chick prototype, 8 nodes
//#define GLOBAL_NUM_THREADS 4096
// Emu Chick prototype, single node
#define GLOBAL_NUM_THREADS 512
#else
#define GLOBAL_NUM_THREADS 4
#endif

#ifdef __le64__
// Emu Chick prototype, single node
#define LOCAL_NUM_THREADS 64
#else
#define LOCAL_NUM_THREADS 4
#endif

static inline long
GLOBAL_GRAIN(long n)
{
    return n > GLOBAL_NUM_THREADS ? (n/GLOBAL_NUM_THREADS) : 1;
}

static inline long
LOCAL_GRAIN(long n)
{
    return n > LOCAL_NUM_THREADS ? (n/LOCAL_NUM_THREADS) : 1;
}