#pragma once
#ifndef __EMU_CC__
#error queue_lock only works on Emu
#endif

#include "common.h"
#include <memoryweb.h>


class queue_lock_mutex
{
private:
    volatile long lock_;
public:
    queue_lock_mutex() : lock_(0) {}
    void lock() {
        do {
            // Spin without doing CAS
            while (lock_ != 0) { RESCHEDULE(); }
            // Try to set to 1, retry if old value was not zero
        } while (0 != ATOMIC_CAS(&lock_, 1L, 0L));
    }
    void unlock() {
        lock_ = 0;
    }
};

// Maximum number of threads that can wait on a single lock
constexpr long max_threads = 2048;

class queue_lock
{
private:
    // Protects access to the queue of waiters
    queue_lock_mutex queue_lock_;
    // The actual state of this lock
    long is_locked_ = 0;
    // Next available position in the queue
    long queue_pos_ = 0;
    // Queue of TSR's that are sleeping while waiting for the lock
    struct tsr { long storage[32]; };
    tsr queue_ [max_threads] = {};
public:

    void lock() {
        // Lock the queue
        queue_lock_.lock();
        if (!is_locked_) {
            // I'm the first one here, just take the lock
            is_locked_ = 1;
            queue_lock_.unlock();
        } else {
            // I'm not the first one here, will go to sleep
            // Reserve a slot in the queue
            long slot = queue_pos_++;
            assert(slot < max_threads);
            // Save my state to the queue
            // STS returns 0 : I am the thread that executed STS
            // STS returns 1 : I am the thread that woke up
            if (!STS(queue_[slot].storage)) {
                // I've saved myself to the queue, give up the lock and die
                queue_lock_.unlock();
                // TODO: make sure we don't give up our credit here
                RELEASE(0, 0);
            }
            // I've been woken up, my turn to enter the critical section
            // Ownership of the _queue_lock was transferred to me
            queue_lock_.unlock();
        }
    }

    void unlock() {
        // Lock the queue
        queue_lock_.lock();
        // Are there any sleeping threads?
        if (queue_pos_ > 0) {
            // Wake up the thread at the tail of the list
            long slot = --queue_pos_;
            WAKEUP(queue_[slot].storage);
            // The thread we just woke up owns the queue_lock now
        } else {
            // Unlock this lock
            is_locked_ = 0;
            // Unlock the queue
            queue_lock_.unlock();
        }
    }
};
