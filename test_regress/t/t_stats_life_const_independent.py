#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test statistics for const propagation (independent rejection)
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_life_const_independent.v"

test.compile(verilator_flags2=['--stats', '--no-timing'])

test.execute()

# Variable is public AND independently rejected by isWrittenByDpi()
# Independent rejection prevents the const propagation opportunity from being counted
# Both counters should be absent (zero values omitted from stats)
test.file_grep_not(test.stats, r'Optimizations, Lifetime constant prop\s+')
test.file_grep_not(test.stats, r'Optimizations, Lifetime constant prop blocked by public\s+')

test.passes()
