#!/usr/bin/env python3
# DESCRIPTION: Verilator: Test statistics for unroll const binding blocked by public_flat_rw
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('vlt')
test.top_filename = "t/t_stats_unroll_existing_binding.v"

test.compile(verilator_flags2=['--stats', '--unroll-count', '1000'])

test.execute()

# Public_flat_rw prevents binding creation
# Verify blocked counter is present
test.file_grep(
    test.stats,
    r'Optimizations, Loop unrolling, Unroll const-fold blocked by public_flat_rw\s+(\d+)')

test.passes()
