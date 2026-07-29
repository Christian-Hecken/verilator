#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test Life assignment deletion with public_flat_rw
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_life_assn_public_rw.v"

test.compile(verilator_flags2=['--stats'])

test.execute()

# Public_flat_rw blocks assignment deletion
test.file_grep(test.stats, r'Optimizations, Lifetime assign deletions blocked by public\s+(\d+)', 1)

test.passes()
