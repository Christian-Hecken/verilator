#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test Dead collision detection with pointer identity
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_dead_collision.v"

test.compile(verilator_flags2=['--stats'])

test.execute()

# Two distinct variables named 'state' in separate modules
# Both are dead and public_flat_rw
# Pointer identity correctly counts them as 2 separate blocked opportunities
test.file_grep(test.stats, r'Optimizations, Dead variables blocked by public\s+(\d+)', 2)

test.passes()
