#!/usr/bin/env python
import os
import json
import itertools
import textwrap

def mkdir(path):
    if not os.path.exists(path):
        os.makedirs(path)


def main():
    emusim = "emusim.HW.x"
    outdir = "pointer_chase4"
    template = textwrap.dedent("""\
    #!/bin/bash
    #SBATCH --job-name={name}
    #SBATCH --output={outdir}/results/{name}.log
    #SBATCH --time=0
    {emusim} \\
    --chick_box \\
    --capture_timing_queues \\
    --output_instruction_count \\
    -o {outdir}/results/{name} \\
    -- \\
    build-sim/{bench}.mwx \\
    --log2_num_elements {log2_num_elements} \\
    --num_threads {num_threads} \\
    --spawn_mode {spawn_mode} \\
    --sort_mode {sort_mode}
    """)

    mkdir("{outdir}/scripts/".format(**locals()))
    mkdir("{outdir}/results/".format(**locals()))

    for bench, spawn_mode, sort_mode, num_threads in itertools.product(
        # bench
        ["pointer_chase"],
        # spawn_mode
        ["serial_spawn", "serial_remote_spawn"],
        # sort_mode
        ["ordered", "shuffled"],
        # num_threads
        [int(2**i) for i in range(0, 8 + 6 + 1)]):

        scale = 25
        log2_num_elements = scale

        # Only generate one serial job
        if spawn_mode == "serial":
            if num_threads == 2:
                num_threads = 1
            else:
                continue

        name = "emusim-chick-box.{}.{}.{}.{}".format(bench, spawn_mode, sort_mode, num_threads)

        with open(os.path.join(outdir, "scripts", name + ".sh"), "w") as f:
            f.write(template.format(**locals()))

if __name__ == "__main__":
    main()

# 64 threads per GC
# 256 threads per nodelet
# 2048 threads per node
# 16K threads per Chick
