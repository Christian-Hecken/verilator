#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test Life constant propagation with public_flat_rw
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_life_const_public_rw.v"

test.compile(verilator_flags2=['--stats'])

test.execute()

# Public_flat_rw blocks constant propagation
test.file_grep(test.stats, r'Optimizations, Lifetime constant prop blocked by public\s+(\d+)', 1)

test.passes()
