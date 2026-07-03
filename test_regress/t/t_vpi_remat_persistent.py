#!/usr/bin/env python3
# DESCRIPTION: Verilator: VPI rematerialized public signal persistent handle test
#
# This file ONLY is placed under the Creative Commons Public Domain, for
# any use, without warranty, 2024 by Wilson Snyder.
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('simulator')

test.compile(
    make_top_shell=False,
    make_main=False,
    make_pli=True,
    verilator_flags2=['--binary', '-CFLAGS \'-O0 -g\'', '--vpi', test.pli_filename],
)

test.execute(check_finished=True)

test.passes()

