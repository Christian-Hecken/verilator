#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test statistics for dead variable elimination (public variant)
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_dead_public.v"

test.compile(verilator_flags2=['--stats'])

# V3Dead runs multiple times per compilation (deadifyModules, deadifyDTypes, etc.)
# The same public variable persists and is recounted each invocation.
# This statistic represents pass-level blocking events, not unique variables.
# Expected: 4 blocking events for the single unused_public variable across all V3Dead passes
test.file_grep(test.stats, r'Optimizations, Dead variables blocked by public\s+(\d+)', 1)

test.passes()
