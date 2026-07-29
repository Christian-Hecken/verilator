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

# Non-public variable allows const binding during unroll
# Loop creates bindings for: i (initial + 3 increments) + x (3 assignments) = 7 total
test.file_grep(test.stats, r'Optimizations, Loop unrolling, Unroll const bindings created\s+(\d+)', 7)
test.file_grep(test.stats, r'Optimizations, Loop unrolling, Unroll const-fold blocked by public_flat_rw\s+(\d+)', 0)

test.passes()