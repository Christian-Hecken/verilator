#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test Dead with public_flat_rd (read-only public)
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_dead_public_rd.v"

test.compile(verilator_flags2=['--stats'])

# All public variants (including public_flat_rd) block dead variable elimination
test.file_grep(test.stats, r'Optimizations, Dead variables blocked by public\s+(\d+)', 1)

# No elimination should occur for any public variant
test.file_grep_not(test.stats, r'Optimizations, Dead variables eliminated')

test.passes()
