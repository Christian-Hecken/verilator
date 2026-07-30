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
test.top_filename = "t/t_stats_inline_port_public_rw.v"

test.compile(verilator_flags2=['--stats', '--inline-mult', '1'])

test.execute()

# Public_flat_rw DOES block port inlining
# Port 'a' is inlined (1), port 'b' is blocked (1)
test.file_grep(test.stats, r'Optimizations, Inline ports inlined\s+(\d+)', 1)
test.file_grep(test.stats, r'Optimizations, Inline ports blocked by public_flat_rw\s+(\d+)', 1)

test.passes()
