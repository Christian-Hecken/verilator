#!/usr/bin/env python3

import vltest_bootstrap

test.scenarios('simulator')

test.compile(make_top_shell=False,
             make_main=False,
             make_pli=True,
             verilator_flags2=["--binary --vpi", test.pli_filename])

test.execute(use_libvpi=True, check_finished=True)

test.passes()
