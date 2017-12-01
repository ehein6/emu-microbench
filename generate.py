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
    outdir = "pointer_chase"
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
    build-hw/{bench}.mwx {mode} {num_elements} {num_threads}
    """)

    mkdir("{outdir}/scripts/".format(**locals()))
    mkdir("{outdir}/results/".format(**locals()))

    for bench, mode, num_elements, num_threads in itertools.product(
        # bench
        ["pointer_chase"],
        # modes
        ["unshuffled", "shuffled"],
        # log2_num_elements
        [23],
        # num_threads
        [int(2**i) for i in range(0, 8 + 6 + 1)]):

        # Only generate one serial job
        if mode == "serial":
            if num_threads == 2:
                num_threads = 1
            else:
                continue

        name = "emusim-chick-box.{}.{}.{}.{}".format(bench, mode, num_elements, num_threads)

        with open(os.path.join(outdir, "scripts", name + ".sh"), "w") as f:
            f.write(template.format(**locals()))

if __name__ == "__main__":
    main()

# 64 threads per GC
# 256 threads per nodelet
# 2048 threads per node
# 16K threads per Chick
