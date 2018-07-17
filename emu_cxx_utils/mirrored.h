#pragma once

#include <utility>
#include <functional>
#include <cstring>

class repl_new
{
public:
    // Overrides default new to always allocate replicated storage for instances of this class
    static void *
    operator new(std::size_t sz)
    {
        return mw_mallocrepl(sz);
    }

    // Overrides default delete to safely free replicated storage
    static void
    operator delete(void * ptr)
    {
        mw_free(ptr);
    }
};

/*
 * Wrapper for replicated object.
 * All writes to this object will be replicated to all nodelets.
 * All reads to this object will be local.
 * Not thread-safe. Only defined for trivially copyable types.
 */
template <
    // Type being replicated
    typename T,
    // Size of type, for partial specialization
    size_t = sizeof(T)
>
// repl<T> behaves as a T with replicated allocation and assignment semantics
class repl : public T, public repl_new
{
//    static_assert(std::is_trivially_copyable<T>::value,
//        "repl<T> only works with trivially copyable classes, maybe you want repl_ctor<T>?");
public:
    // Returns a reference to the copy of T on the Nth nodelet
    T& get_nth(long n)
    {
        assert(n < NODELETS());
        return *static_cast<T*>(mw_get_nth(this, n));
    }

    // Wrapper constructor to copy T to each nodelet after running the requested constructor
    template<typename... Args>
    explicit repl (Args&&... args)
    // Call T's constructor with forwarded args
    : T(std::forward<Args>(args)...)
    {
        // Get pointer to constructed T
        T* local = &get_nth(NODE_ID());
        // Replicate to each remote nodelet
        for (long i = 0; i < NODELETS(); ++i) {
            T * remote = &get_nth(i);
            if (local == remote) { continue; }
            // Copy local to remote
            memcpy(remote, local, sizeof(T));
        }
    }

    // Initializes all copies to the same value
    void
    operator=(const T& rhs)
    {
        for (long i = 0; i < NODELETS(); ++i) {
            T * remote = &get_nth(i);
            // Assign value to remote copy
            memcpy(remote, rhs, sizeof(T));
        }
    }
};

// More efficient version for 8-byte types, avoids calling memcpy
template<typename T>
class repl<T, 8> : public repl_new
{
    // I'm assuming here that all pointers are 8 bytes, so I can cast them to/from long
    // without losing anything. This is true on Emu and most 64-bit machines, but not in general.
    static_assert(sizeof(long) == 8,
        "This code wasn't designed for systems with a 32-bit long type.");
    static_assert(std::is_trivially_copyable<T>::value,
        "repl<T> only works with trivially copyable classes, maybe you want repl_ctor<T>?");
private:
    T val;
public:
    // Returns a reference to the copy of T on the Nth nodelet
    T& get_nth(long n)
    {
        assert(n < NODELETS());
        return *static_cast<T*>(mw_get_nth(this, n));
    }

    // Default constructor
    repl<T,8>() = default;

    // Wrapper constructor to copy T to each nodelet after running the requested constructor
    // Call T's constructor with forwarded args
    repl<T,8>(T x)
    {
        mw_replicated_init(reinterpret_cast<long*>(&val), reinterpret_cast<long>(x));
    }

    operator T& ()
    {
        return val;
    }

    // Initializes all copies to the same value
    void
    operator=(T rhs)
    {
        mw_replicated_init(reinterpret_cast<long*>(&val), reinterpret_cast<long>(rhs));
    }

    // If T is a pointer type, allow users to dereference it
    template<typename U = T>
    typename std::enable_if<std::is_pointer<U>::value, U>::type
    operator->() { return val; }
};

/*
 * Constructs a copy of T in replicated storage on each nodelet.
 * Destructs each T when it goes out of scope.
 * Reads, writes and method calls are performed on the local copy.
 */
template<typename T>
class repl_ctor : public T, public repl_new
{
public:
    // Returns a reference to the copy of T on the Nth nodelet
    T& get_nth(long n)
    {
        assert(n < NODELETS());
        return *static_cast<T*>(mw_get_nth(this, n));
    }

    // Constructor template - allows repl_ctor<T> to be constructed just like a T
    template<typename... Args>
    explicit repl_ctor(Args&&... args)
    // Call T's constructor with forwarded args
    : T(std::forward<Args>(args)...)
    {
        // Pointer to the object on this nodelet, which has already been constructed
        T * local = &get_nth(NODE_ID());
        // For each nodelet...
        for (long n = 0; n < NODELETS(); ++n) {
            // Use placement-new to construct each remote object with forwarded arguments
            T * remote = &get_nth(n);
            if (local == remote) continue;
            new (remote) T(std::forward<Args>(args)...);
        }
    }

    ~repl_ctor()
    {
        // Pointer to the object on this nodelet, which has already been destructed
        T * local = &get_nth(NODE_ID());
        // For each nodelet...
        for (long n = 0; n < NODELETS(); ++n) {
            // Explicitly call destructor to tear down each remote object
            T * remote = &get_nth(n);
            if (local == remote) continue;
            remote->~T();
        }
    }
};
