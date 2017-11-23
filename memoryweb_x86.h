#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Mimic memoryweb behavior on x86
// TODO eventually move this all to its own header file
#define NODE_ID() (0)
#define NODELETS() (1)
#define replicated
#define PRIORITY(X) (63-__builtin_clzl(X))
#define MIGRATE(X)
#define noinline __attribute__ ((noinline))

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

#ifdef __cplusplus
}
#endif