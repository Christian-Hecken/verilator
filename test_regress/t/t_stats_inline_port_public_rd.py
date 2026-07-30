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
test.top_filename = "t/t_stats_inline_port_public_rd.v"

test.compile(verilator_flags2=['--stats', '--inline-mult', '1'])

test.execute()

# Public_flat_rd does NOT block port inlining (only public_flat_rw blocks)
# 2 ports (a, b) should still be inlined
test.file_grep(test.stats, r'Optimizations, Inline ports inlined\s+(\d+)', 2)
test.file_grep(test.stats, r'Optimizations, Inline ports blocked by public_flat_rw\s+(\d+)', 0)

test.passes()
