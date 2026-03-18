#!/usr/bin/env python3

import vltest_bootstrap

test.scenarios('simulator')

test.compile(make_top_shell=False, make_main=False, verilator_flags2=["--binary"])

test.execute(check_finished=True)

test.passes()
