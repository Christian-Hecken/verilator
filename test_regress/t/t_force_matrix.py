#!/usr/bin/env python3
"""
Minimal test_regress driver for t_force_matrix.

This mirrors the minimal style used by other `t_*` drivers and delegates
building to the test harness. The detailed matrix tests will be implemented
inside the C++/SV code and PLI/DPI helpers.
"""

import os
import vltest_bootstrap

test.scenarios('simulator')

test.compile(make_top_shell=False,
             make_main=False,
             make_pli=False,
             verilator_flags2=["--exe", "t/t_force_matrix.cpp", "--no-timing", "--vpi"])

result = test.execute(use_libvpi=True, check_finished=True)
# Print matrix test results from simulation output
print("\n=== t_force_matrix matrix test results ===")
logpath = os.path.join('obj_vlt', 't_force_matrix', 'vlt_sim.log')
if os.path.exists(logpath):
    with open(logpath, 'r') as f:
        for line in f:
            if 't_force_matrix: Test Results' in line or '[Write]' in line or '[Force]' in line or '[Release]' in line:
                print(line.strip())
else:
    print('WARNING: log not found:', logpath)
test.passes()
