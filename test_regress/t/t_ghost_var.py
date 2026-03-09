#!/usr/bin/env python3
# DESCRIPTION: Verilator: Ghost variable optimization test
#
# This file ONLY is placed under the Creative Commons Public Domain.
# SPDX-FileCopyrightText: 2026 Verilator Contributors
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('simulator')

test.compile(make_top_shell=False, make_main=False, verilator_flags2=["--exe", test.pli_filename])

test.execute()

test.passes()
