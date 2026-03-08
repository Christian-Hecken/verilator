#!/usr/bin/env python3
# DESCRIPTION: Regression test for alias-aware public_flat_rw variable registration
# (ensure VPI writes propagate through aliases after inlining/elimination)

import vltest_bootstrap


test.scenarios('simulator')

test.compile(make_top_shell=False,
             make_main=False,
             make_pli=True,
             iv_flags2=["-g2005-sv"],
             verilator_flags2=["--exe --vpi --no-l2name --public-flat-rw", "t/t_vpi_alias.cpp"])

# the C++ driver checks correctness internally

test.execute(use_libvpi=True)

test.passes()
