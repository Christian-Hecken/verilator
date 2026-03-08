#!/usr/bin/env python3
# DESCRIPTION: Microbenchmark for dead variable elimination (with --public-flat-rw)
# With global public, V3Gate cannot remove dead drivers and V3Dead cannot prune
# the unreferenced variables. Compare statistics against t_dead_elim.py.

import vltest_bootstrap

test.scenarios('simulator')

test.top_filename = "t/t_dead_elim.v"

test.compile(make_top_shell=False,
             make_main=False,
             verilator_flags2=[
                 "--binary", "-DDEPTH=4096", "-DSIM_CYCLES=2_000_000", "--public-flat-rw",
                 "--stats"
             ])

test.execute()

test.passes()
