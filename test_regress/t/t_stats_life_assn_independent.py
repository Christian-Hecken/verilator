#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test statistics for assignment deletion (independent rejection)
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_life_assn_independent.v"

test.compile(verilator_flags2=['--stats'])

test.execute()

# Variable is public AND independently rejected by isReadByDpi()
# Independent rejection prevents the assignment deletion opportunity from being counted
# Both counters should be absent (zero values omitted from stats)
test.file_grep_not(test.stats, r'Optimizations, Lifetime assign deletions\s+')
test.file_grep_not(test.stats, r'Optimizations, Lifetime assign deletions blocked by public\s+')

test.passes()
