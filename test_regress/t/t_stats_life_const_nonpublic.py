#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2024 by Wilson Snyder. This program is free software; you
# can redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('simulator')
test.top_filename = "t/t_stats_life_const_nonpublic.v"

test.compile(verilator_flags2=["--stats"])

test.execute()

# Non-public variable: constant propagation should succeed
test.file_grep(test.stats, r'Optimizations, Lifetime constant prop\s+(\d+)', 1)
# Blocked counter should not appear (zero values are omitted from stats)
if test.vlt:
    test.file_grep_not(test.stats, r'Optimizations, Lifetime constant prop blocked by public')

test.passes()
