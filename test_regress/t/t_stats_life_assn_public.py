#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test statistics for assignment deletion
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_life_assn_public.v"

test.compile(verilator_flags2=['--stats'])

# Exact counts: 1 non-public redundant assignment deleted, 2 public blocked
test.file_grep(test.stats, r'Optimizations, Lifetime assign deletions\s+(\d+)', 1)
test.file_grep(test.stats, r'Optimizations, Lifetime assign deletions blocked by public\s+(\d+)', 2)

test.passes()
