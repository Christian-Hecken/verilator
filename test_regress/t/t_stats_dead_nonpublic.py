#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test statistics for dead variable elimination (non-public baseline)
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_dead_nonpublic.v"

test.compile(verilator_flags2=['--stats'])

# Non-public dead variable should be eliminated
test.file_grep(test.stats, r'Optimizations, Dead variables eliminated\s+(\d+)', 1)

# No public blocking should occur in the non-public baseline
test.file_grep_not(test.stats, r'Optimizations, Dead variables blocked by public')

test.passes()
