#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test Life assignment deletion collision detection
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_life_assn_collision.v"

test.compile(verilator_flags2=['--stats'])

test.execute()

# Two distinct deletable assignments in separate modules
# Both are blocked by public_flat_rw
# Pointer identity correctly counts them as 2 separate blocked opportunities
test.file_grep(test.stats, r'Optimizations, Lifetime assign deletions blocked by public\s+(\d+)',
               2)

test.passes()
