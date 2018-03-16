#pragma once

/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])
    declaration = Template("""
        long
        emu_1d_array_reduce_sum_v${num_args}(long * array, long size, long grain,
            void (*worker)(long * array, long begin, long end, long * sum${arg_decls})
            ${arg_decls}
        );
    """)
    cog.out(declaration.substitute(**locals()), dedent=True, trimblanklines=True)

]]]*/
long
emu_1d_array_reduce_sum_v0(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum)
    
);
long
emu_1d_array_reduce_sum_v1(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1)
    , void * arg1
);
long
emu_1d_array_reduce_sum_v2(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1, void * arg2)
    , void * arg1, void * arg2
);
long
emu_1d_array_reduce_sum_v3(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3
);
long
emu_1d_array_reduce_sum_v4(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4
);
long
emu_1d_array_reduce_sum_v5(long * array, long size, long grain,
    void (*worker)(long * array, long begin, long end, long * sum, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5
);
/* [[[end]]] */