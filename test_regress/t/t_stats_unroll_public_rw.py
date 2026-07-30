#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2024 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('vlt')

test.compile(verilator_flags2=["--stats", "--unroll-count", "10"])

test.execute()

# Public_flat_rw variable blocks const binding for x assignments
# Loop variable i can still be bound: initial + 3 increments = 4 bindings
# Variable x assignments are blocked: 3 blocked
test.file_grep(test.stats, r'Optimizations, Loop unrolling, Unroll const bindings created\s+(\d+)',
               4)
test.file_grep(
    test.stats,
    r'Optimizations, Loop unrolling, Unroll const-fold blocked by public_flat_rw\s+(\d+)', 3)

test.passes()
