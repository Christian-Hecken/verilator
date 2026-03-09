#!/usr/bin/env python3
# DESCRIPTION: Microbenchmark for ghost variable optimization (baseline, no public)

import vltest_bootstrap

test.scenarios('simulator')

test.top_filename = "t/t_ghost_perf.v"

test.compile(make_top_shell=False,
             make_main=False,
             verilator_flags2=["--binary", "-DDEPTH=4096", "-DSIM_CYCLES=2_000_000"])

test.execute()

test.passes()
