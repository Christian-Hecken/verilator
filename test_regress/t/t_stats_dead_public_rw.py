#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test Dead with public_flat_rw (read-write public)
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_dead_public_rw.v"

test.compile(verilator_flags2=['--stats'])

# Public read-write blocks dead variable elimination
test.file_grep(test.stats, r'Optimizations, Dead variables blocked by public\s+(\d+)', 1)

# No elimination should occur for public_flat_rw
test.file_grep_not(test.stats, r'Optimizations, Dead variables eliminated')

test.passes()
