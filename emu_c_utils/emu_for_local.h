#pragma once

// TODO get max number of threads on platform
#ifdef __le64__
#define LOCAL_GRAIN(X) (X/64)
#else
#define LOCAL_GRAIN(X) (X/4)
#endif

/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])

    function=Template("""
        void
        emu_local_for_v${num_args}(long begin, long end, long grain,
            void (*worker)(long begin, long end${arg_decls})
            ${arg_decls});

    """)
    cog.out(function.substitute(**locals()), dedent=True, trimblanklines=True)
]]]*/
void
emu_local_for_v0(long begin, long end, long grain,
    void (*worker)(long begin, long end)
    );

void
emu_local_for_v1(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1)
    , void * arg1);

void
emu_local_for_v2(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2);

void
emu_local_for_v3(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3);

void
emu_local_for_v4(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4);

void
emu_local_for_v5(long begin, long end, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5);

/* [[[end]]] */
