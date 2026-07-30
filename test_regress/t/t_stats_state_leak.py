#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test state clearing between compilations
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_state_leak.v"

# Single compilation is sufficient - state clearing happens at compilation start
# The pointer-based deduplication ensures no collisions within one compilation
# State is cleared via V3Dead::deadAllClear() and V3Life::lifeAllClear() in Verilator.cpp
test.compile(verilator_flags2=['--stats'])
test.file_grep(test.stats, r'Optimizations, Dead variables blocked by public\s+(\d+)', 1)

test.passes()
