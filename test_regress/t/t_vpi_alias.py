#!/usr/bin/env python3
# DESCRIPTION: Regression test for alias-aware public_flat_rw variable registration
# (ensure VPI reads return correct values through aliases after inlining/elimination)

import vltest_bootstrap

test.scenarios('simulator')

test.compile(make_top_shell=False,
             make_main=False,
             verilator_flags2=["--exe", "--vpi", "--public-flat-rw", "t/t_vpi_alias.cpp"])

test.execute()

test.passes()
