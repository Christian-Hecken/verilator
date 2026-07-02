#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2024 by Wilson Snyder. This program is free software; you can
# redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('vltmt')
test.top_filename = "t/t_vpi_public_remat.v"

test.compile(make_top_shell=False,
             make_main=False,
             verilator_flags2=[
                 "--vpi",
                 "--timing",
                 "--binary",
                 test.t_dir + "/t_vpi_public_remat.cpp",
             ],
             threads=8)

test.execute()

test.passes()
