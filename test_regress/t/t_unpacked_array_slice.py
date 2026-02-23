#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2025 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('simulator')

# Test unpacked array slice with multi-dimensional indexing
# e.g., my_mem[0:1][3] where [0:1] is a slice on the first dimension
# and [3] is an index on the second dimension.
# IEEE 1800-2017 Section 7.4.6

test.compile(
    fails=test.vlt_all,  # Verilator unsupported: multi-dim unpacked array slice
    expect_filename=test.golden_filename)

if not test.vlt_all:
    test.execute()

test.passes()
