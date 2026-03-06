#!/usr/bin/env python3
# DESCRIPTION: Microbenchmark for dead variable elimination (baseline, no public)
# Signals that are written but never read should be pruned by V3Gate (GateUnused)
# and V3Dead. Compare statistics against t_dead_elim_public.py.

import vltest_bootstrap

test.scenarios('simulator')

test.top_filename = "t/t_dead_elim.v"

test.compile(make_top_shell=False,
             make_main=False,
             verilator_flags2=["--binary", "-DDEPTH=4096", "-DSIM_CYCLES=2_000_000", "--stats"])

test.execute()

test.passes()
