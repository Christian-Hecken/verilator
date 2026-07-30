#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test statistics for variable localization (string type independent rejection)
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_localize_string_public.v"

test.compile(verilator_flags2=["--stats"])

test.execute()

# Variable is public AND is string type - independently rejected
# String type check happens before public check in isOptimizable(), so independent rejection
# Neither counter should increment for this variable (both should be 0)
test.file_grep(test.stats, r'Optimizations, Vars localized\s+(\d+)', 0)
test.file_grep(test.stats, r'Optimizations, Vars localization blocked by public\s+(\d+)', 0)

test.passes()
