#pragma once


/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])
    declaration = Template("""
        void
        emu_1d_array_apply_v${num_args}(long * array, long size, long grain,
            void (*worker)(long * array, long begin, long end${arg_decls})
            ${arg_decls}
        );
    """)
    cog.out(declaration.substitute(**locals()), dedent=True, trimblanklines=True)

]]]*/
void
emu_1d_array_apply_v0(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end)
    
);
void
emu_1d_array_apply_v1(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1)
    , void * arg1
);
void
emu_1d_array_apply_v2(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2
);
void
emu_1d_array_apply_v3(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3
);
void
emu_1d_array_apply_v4(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4
);
void
emu_1d_array_apply_v5(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5
);
/* [[[end]]] */

// TODO generate this macro with the cog code above
#define GET_EMU_1D_ARRAY_APPLY_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, _9, NAME, ...) NAME
#define EMU_1D_ARRAY_APPLY(...) GET_EMU_1D_ARRAY_APPLY_MACRO(__VA_ARGS__, \
emu_1d_array_apply_v5, \
emu_1d_array_apply_v4, \
emu_1d_array_apply_v3, \
emu_1d_array_apply_v2, \
emu_1d_array_apply_v1, \
emu_1d_array_apply_v0) \
(__VA_ARGS__)
