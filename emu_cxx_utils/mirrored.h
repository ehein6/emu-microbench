#pragma once

#include <utility>
#include <functional>
#include <cstring>

/*
 * This header provides support for storing C++ objects in replicated memory.
 * There are several template wrappers to choose from depending on what kind of behavior you want
 * - repl_new     : Classes that inherit from repl_new will be allocated in replicated memory.
 *
 * - repl<T>      : Call constructor locally, then shallow copy onto each remote nodelet.
 *                  The destructor will be called on only one copy, the other shallow copies ARE NOT DESTRUCTED.
 *                  The semantics of "shallow copy" depend on the type of T
 *                      - If T is trivially copyable (no nested objects or custom copy constructor),
 *                        remote copies will be initialized using memcpy()
 *                      - If T defines a "shallow copy constructor", this will be used to construct shallow copies
 *                        The "shallow copy constructor" has the following signature:
 *                            T(const T& other, bool)
 *                        The second argument can be ignored.
 *                      - Alternatively, if shallow_copy(T* dst, const T* src) is defined, this will be used instead.
 *                  Note that shallow
 *
 * - repl_ctor<T> : Call constructor on every nodelet with same arguments.
 *                  Every copy will be destructed individually.
 * - repl_copy<T> : Construct locally, then copy-construct onto each remote nodelet.
 *                  Every copy will be destructed individually.
 */

namespace emu {

/**
 * Overrides default new to always allocate replicated storage for instances of this class.
 * repl_new is intended to be used as a parent class for distributed data structure types.
 */
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


/**
 * repl<T> : Wrapper template to add replicated semantics to a class using shallow copies.
 *
 * Assignment:
 *     Will call the assignment operator on each remote copy.
 *
 * Construction:
 *     Will call T's constructor locally, then shallow copy T onto each remote nodelet.
 *     The semantics of "shallow copy" depend on the type of T.
 *
 *    - If T is trivially copyable (no nested objects or custom copy constructor),
 *    remote copies will be initialized using memcpy().
 *
 *    - If T defines a "shallow copy constructor", this will be used to construct shallow copies.
 *    The "shallow copy constructor" has the following signature:
 *     @code T(const T& other, bool) @endcode
 *    The second argument can be ignored.
 *
 *    - Alternatively, if @code shallow_copy(T* dst, const T* src) @endcode is defined,
 *    this will be used instead.
 *
 * Destruction:
 *     The destructor will be called on only one copy, the other shallow copies ARE NOT DESTRUCTED.
 *
 * All other operations (function calls, attribute accesses) will access the local copy of T.
 */
template <typename T>
class repl_shallow : public T, public repl_new
{
public:
/**
 * Default shallow copy operation to be used by repl<T>
 * Only defined for trivially copyable (i.e. copy constructor == memcpy) types.
 * In this case there is no difference between a deep copy and a shallow copy.
 * @tparam T Type with trivial copy semantics
 * @param dst Pointer to sizeof(T) bytes of uninitialized memory
 * @param src Pointer to constructed T
 */
    template<typename U=T>
    void
    shallow_copy(
        typename std::enable_if<std::is_trivially_copyable<U>::value, U>::type * dst,
        const U * src)
    {
        memcpy(dst, src, sizeof(U));
    }

    /**
     * If T has a "shallow copy constructor" (copy constructor with additional dummy bool argument)
     * repl<T> will use this version instead. Otherwise SFINAE will make this one go away.
     * @tparam T Type with a "shallow copy constructor" (copy constructor with additional dummy bool argument)
     * @param dst Pointer to sizeof(T) bytes of uninitialized memory
     * @param src Pointer to constructed T
     */
    template<typename U=T>
    void
    shallow_copy(
        U * dst,
        const U * src)
    {
        const bool shallow = true;
        new (dst) U(*src, shallow);
    }

    /**
     * Returns a reference to the copy of T on the Nth nodelet
     * @param n nodelet ID
     * @return Reference the copy of T on the Nth nodelet
     */
    T& get_nth(long n)
    {
        assert(n < NODELETS());
        return *static_cast<T*>(mw_get_nth(this, n));
    }

    // Wrapper constructor to copy T to each nodelet after running the requested constructor
    template<typename... Args>
    explicit repl_shallow (Args&&... args)
    // Call T's constructor with forwarded args
    : T(std::forward<Args>(args)...)
    {
        // Get pointer to constructed T
        T* local = &get_nth(NODE_ID());
        // Replicate to each remote nodelet
        for (long i = 0; i < NODELETS(); ++i) {
            T * remote = &get_nth(i);
            if (local == remote) { continue; }
            // Shallow copy copy local to remote
            shallow_copy(remote, local);
        }
    }

    // Initializes all copies to the same value
    repl_shallow&
    operator=(const T& rhs)
    {
        for (long i = 0; i < NODELETS(); ++i) {
            T * remote = &get_nth(i);
            // Assign value to remote copy
            *remote = rhs;
        }
        return *this;
    }
};

/**
 * Partial specialization of repl<T> for integral types.
 * Need a slightly different implementation here since we can't inherit from primitives.
 */
template<typename T>
class repl : public repl_new
{
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
    repl<T>() = default;

    // Wrapper constructor to copy T to each nodelet after running the requested constructor
    // Call T's constructor with forwarded args
    repl<T>(T x)
    {
        mw_replicated_init(reinterpret_cast<long*>(&val), reinterpret_cast<long>(x));
    }

    operator T& ()
    {
        return val;
    }

    // Initializes all copies to the same value
    repl&
    operator=(T rhs)
    {
        mw_replicated_init(reinterpret_cast<long*>(&val), reinterpret_cast<long>(rhs));
        return *this;
    }

    // If T is a pointer type, allow users to dereference it
    template<typename U = T>
    typename std::enable_if<std::is_pointer<U>::value, U>::type
    operator->() { return val; }
};

/**
 * repl_ctor<T> : Wrapper template to add replicated semantics to a class using distributed construction.
 *
 * Assignment:
 *     Will call the assignment operator on the local copy.
 *
 * Construction:
 *     Calls constructor on every nodelet's copy with the same arguments.
 *
 * Destruction:
 *     The destructor will be called on each copy individually.
 *
 * All other operations (function calls, attribute accesses) will access the local copy of T.
 */
template<typename T>
class repl_ctor : public T, public repl_new
{
public:
    /**
     * Returns a reference to the copy of T on the Nth nodelet
     * @param n nodelet ID
     * @return Returns a reference to the copy of T on the Nth nodelet
     */
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


/**
 * repl_ctor<T> : Wrapper template to add replicated semantics to a class using distributed copy construction.
 *
 * Assignment:
 *     Will call the assignment operator on the local copy.
 *
 * Construction:
 *     Calls constructor on local nodelet, then copy-constructs to each remote nodelet.
 *
 * Destruction:
 *     The destructor will be called on each copy individually.
 *
 * All other operations (function calls, attribute accesses) will access the local copy of T.
 */
template<typename T>
class repl_copy : public T, public repl_new
{
public:
    /**
     * Returns a reference to the copy of T on the Nth nodelet
     * @param n nodelet ID
     * @return Returns a reference to the copy of T on the Nth nodelet
     */
    T& get_nth(long n)
    {
        assert(n < NODELETS());
        return *static_cast<T*>(mw_get_nth(this, n));
    }

    // Constructor template - allows repl_ctor<T> to be constructed just like a T
    template<typename... Args>
    explicit repl_copy(Args&&... args)
    // Call T's constructor with forwarded args
    : T(std::forward<Args>(args)...)
    {
        // Pointer to the object on this nodelet, which has already been constructed
        T * local = &get_nth(NODE_ID());
        // For each nodelet...
        for (long n = 0; n < NODELETS(); ++n) {
            // Use copy constructor  to construct each remote object
            T * remote = &get_nth(n);
            if (local == remote) continue;
            new (remote) T(*local);
        }
    }

    ~repl_copy()
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

} // end namespace emu