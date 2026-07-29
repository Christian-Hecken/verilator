#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test Life assignment deletion with public_flat_rd
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_life_assn_public_rd.v"

test.compile(verilator_flags2=['--stats'])

test.execute()

# Public_flat_rd blocks assignment deletion
test.file_grep(test.stats, r'Optimizations, Lifetime assign deletions blocked by public\s+(\d+)', 1)

test.passes()
