#!/usr/bin/env python3
# DESCRIPTION: Regression to exercise aliasing / public signal overhead (global public)

import vltest_bootstrap

test.scenarios('simulator')

test.top_filename = "t/t_aliasing.v"

test.compile(make_top_shell=False,
             make_main=False,
             verilator_flags2=["--binary", "-DDEPTH=4096", "-DSIM_CYCLES=2_000_000", "--public-flat-rw", "--stats"])

test.execute()

test.passes()
