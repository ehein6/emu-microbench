#pragma once

/*[[[cog

from string import Template

for num_args in xrange(6):

    arg_decls = "".join([", void * arg%i"%(i+1) for i in xrange(num_args)])
    declaration = Template("""
        void
        emu_chunked_array_apply_v${num_args}(
            void ** array, long num_elements, long grain,
            void (*worker)(long begin, long end${arg_decls})
            ${arg_decls});
    """)
    cog.out(declaration.substitute(**locals()), dedent=True, trimblanklines=True)

]]]*/
void
emu_chunked_array_apply_v0(
    void ** array, long num_elements, long grain,
    void (*worker)(long begin, long end)
    );
void
emu_chunked_array_apply_v1(
    void ** array, long num_elements, long grain,
    void (*worker)(long begin, long end, void * arg1)
    , void * arg1);
void
emu_chunked_array_apply_v2(
    void ** array, long num_elements, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2)
    , void * arg1, void * arg2);
void
emu_chunked_array_apply_v3(
    void ** array, long num_elements, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3)
    , void * arg1, void * arg2, void * arg3);
void
emu_chunked_array_apply_v4(
    void ** array, long num_elements, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4)
    , void * arg1, void * arg2, void * arg3, void * arg4);
void
emu_chunked_array_apply_v5(
    void ** array, long num_elements, long grain,
    void (*worker)(long begin, long end, void * arg1, void * arg2, void * arg3, void * arg4, void * arg5)
    , void * arg1, void * arg2, void * arg3, void * arg4, void * arg5);
/* [[[end]]] */
