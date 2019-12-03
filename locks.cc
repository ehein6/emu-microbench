#include <cstdlib>
#include <cstdio>
#include <string>
#include <cilk/cilk.h>

#include <emu_c_utils/emu_c_utils.h>

#include "common.h"
#if defined(__EMU_CC__) && defined(WAKEUP)
#include "queue_lock.h"
#endif

// Define type-safe wrappers for Emu atomic intrinsics
// TODO Include these from emu_cxx_utils
namespace emu {
// Increment 64-bit int
inline long
atomic_addms(volatile long *ptr, long value) {
    return ATOMIC_ADDMS(ptr, value);
}

// Increment pointer
template<class T>
inline T *
atomic_addms(T *volatile *ptr, ptrdiff_t value) {
    value *= sizeof(T);
    return (T *) ATOMIC_ADDMS((volatile long *) ptr, (long) value);
}


template<class T>
inline T
atomic_cas(T volatile *ptr, T oldval, T newval);

template<>
inline long
atomic_cas(long volatile *ptr, long oldval, long newval) {
    return ATOMIC_CAS(ptr, newval, oldval);
}

template<typename T>
inline T
atomic_cas(T volatile *ptr, T oldval, T newval) {
    static_assert(sizeof(T) == sizeof(long), "CAS supported only for 64-bit types");
    // We're fighting the C++ type system, but codegen doesn't care
    union pun {
        T t;
        long i;
    };
    pun oldval_p{oldval};
    pun newval_p{newval};
    pun retval_p;

    retval_p.i = ATOMIC_CAS(
        reinterpret_cast<long volatile *>(ptr),
        newval_p.i,
        oldval_p.i
    );
    return retval_p.t;
}
}

// Spinlock using ATOMIC_CAS
class cas_mutex_A
{
private:
    volatile long lock_;
public:
    cas_mutex_A() : lock_(0) {}
    void lock() {
        // Try to set to 1, retry if old value was not zero
        while (0 != emu::atomic_cas(&lock_, 0L, 1L));
    }
    void unlock() {
        lock_ = 0;
    }
};

void lock_cas_mutex_A();

// Spinlock using LD and ATOMIC_CAS
class cas_mutex_B
{
private:
    volatile long lock_;
public:
    cas_mutex_B() : lock_(0) {}
    void lock() {
        do {
            // Spin without doing CAS
            while (lock_ != 0);
            // Try to set to 1, retry if old value was not zero
        } while (0 != emu::atomic_cas(&lock_, 0L, 1L));
    }
    void unlock() {
        lock_ = 0;
    }
};

// Spinlock using LD and ATOMIC_CAS with reschedule
class cas_mutex_C
{
private:
    volatile long lock_;
public:
    cas_mutex_C() : lock_(0) {}
    void lock() {
        do {
            // Spin without doing CAS
            while (lock_ != 0) { RESCHEDULE(); }
            // Try to set to 1, retry if old value was not zero
        } while (0 != emu::atomic_cas(&lock_, 0L, 1L));
    }
    void unlock() {
        lock_ = 0;
    }
};

#ifdef __EMU_CC__
// Defined in lock_impls.S
extern "C" void lock_cas_mutex_D(volatile long * lock);
class cas_mutex_D
{
private:
    volatile long lock_;
public:
    cas_mutex_D() : lock_(0) {}
    void lock() { lock_cas_mutex_D(&lock_); }
    void unlock() { lock_ = 0; }
};

// Defined in lock_impls.S
extern "C" void lock_cas_mutex_E(volatile long * lock);
class cas_mutex_E
{
private:
    volatile long lock_;
public:
    cas_mutex_E() : lock_(0) {}
    void lock() { lock_cas_mutex_E(&lock_); }
    void unlock() { lock_ = 0; }
};

// Defined in lock_impls.S
extern "C" void lock_cas_mutex_F(volatile long * lock);
class cas_mutex_F
{
private:
    volatile long lock_;
public:
    cas_mutex_F() : lock_(0) {}
    void lock() { lock_cas_mutex_F(&lock_); }
    void unlock() { lock_ = 0; }
};
#endif


template<class Mutex>
noinline void
worker(Mutex& mutex, volatile double * counter, long n)
{
    // Lock and increment counter N times
    for (long i = 0; i < n; ++i){
        mutex.lock();
        *counter += 1.0;
        mutex.unlock();
    }
}

#ifdef __EMU_CC__
template<> void worker(cas_mutex_D& mutex, volatile double * counter, long n);
template<> void worker(cas_mutex_E& mutex, volatile double * counter, long n);
template<> void worker(cas_mutex_F& mutex, volatile double * counter, long n);
#endif

template<typename Mutex>
void run_test(long n, long num_threads, long num_trials)
{
    long n_per_thread = n / num_threads;
    if (n_per_thread == 0) {
        LOG("ERROR: N must be divisible by num_threads\n");
        exit(1);
    }
    Mutex mutex;
    volatile double counter;
    for (long trial = 0; trial < num_trials; ++trial) {
        counter = 0;
        hooks_set_attr_i64("trial", trial);
        hooks_region_begin("allocation");
        for (long i = 0; i < num_threads; ++i){
            cilk_spawn worker(mutex, &counter, n_per_thread);
        }
        cilk_sync;
        double time_ms = hooks_region_end();
        double locks_per_second = n / (time_ms/1000);
        LOG("%3.2f million lock acquires per second\n",
            locks_per_second / (1000000));

#ifndef NO_VALIDATE
        long counter_val = static_cast<long>(counter);
        if (counter_val != n) {
            LOG("ERROR: Counter mismatch (%li != %li)\n", counter_val, n);
        }
#endif
    }
}

int main(int argc, char** argv)
{
    struct {
        std::string impl;
        long log2_n;
        long num_threads;
        long num_trials;
    } args;

    if (argc != 5) {
        LOG("Usage: %s impl log2_n num_threads num_trials\n", argv[0]);
        LOG("    impl can be 'all' or one of the following:\n");
        LOG("    cas_mutex_{A,B,C,D,E,F}, queue_lock\n");
        exit(1);
    } else {
        args.impl = argv[1];
        args.log2_n = atol(argv[2]);
        args.num_threads = atol(argv[3]);
        args.num_trials = atol(argv[4]);

        if (args.log2_n < 0) { LOG("log2_n must be >= 0\n"); exit(1); }
        if (args.num_threads <= 0) { LOG("num_threads must be > 0\n"); exit(1); }
        if (args.num_trials <= 0) { LOG("num_trials must be > 0\n"); exit(1); }
    }

    hooks_set_attr_i64("log2_num_mallocs", args.log2_n);
    hooks_set_attr_i64("num_threads", args.num_threads);

    long n = 1L << args.log2_n;

    LOG("Testing with %li threads, total of %li lock/unlock operations\n",
        args.num_threads, n);


#define RUN_BENCHMARK(NAME) \
    LOG("Benchmarking %s:\n", #NAME); \
    hooks_set_attr_str("mutex", #NAME); \
    run_test<NAME>(n, args.num_threads, args.num_trials);

    if (args.impl == "all") {
        RUN_BENCHMARK(cas_mutex_A);
        RUN_BENCHMARK(cas_mutex_B);
        RUN_BENCHMARK(cas_mutex_C);
#ifdef __EMU_CC__
        RUN_BENCHMARK(cas_mutex_D);
        RUN_BENCHMARK(cas_mutex_E);
        RUN_BENCHMARK(cas_mutex_F);
#ifdef WAKEUP
        RUN_BENCHMARK(queue_lock);
#endif
#endif
    } else if (args.impl == "cas_mutex_A") {
        RUN_BENCHMARK(cas_mutex_A);
    } else if (args.impl == "cas_mutex_B") {
        RUN_BENCHMARK(cas_mutex_B);
    } else if (args.impl == "cas_mutex_C") {
        RUN_BENCHMARK(cas_mutex_C);
#ifdef __EMU_CC__
    } else if (args.impl == "cas_mutex_D") {
        RUN_BENCHMARK(cas_mutex_D);
    } else if (args.impl == "cas_mutex_E") {
        RUN_BENCHMARK(cas_mutex_E);
    } else if (args.impl == "cas_mutex_F") {
        RUN_BENCHMARK(cas_mutex_F);
#ifdef WAKEUP
    } else if (args.impl == "queue_lock") {
        RUN_BENCHMARK(queue_lock);
#endif
#endif
    } else {
        LOG("'%s' is not implemented!\n", args.impl.c_str());
        exit(1);
    }

    return 0;
}
