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

# Variable is both forced AND public_flat_rw
# Forced check happens before public check in isForced() condition
# Loop variable i can still be bound: initial + 3 increments = 4 bindings
# Variable x assignments blocked by force (not counted as public block): 0 blocked by public
test.file_grep(test.stats, r'Optimizations, Loop unrolling, Unroll const bindings created\s+(\d+)', 4)
test.file_grep(test.stats, r'Optimizations, Loop unrolling, Unroll const-fold blocked by public_flat_rw\s+(\d+)', 0)

test.passes()