#!/usr/bin/env python3

import vltest_bootstrap

test.scenarios('simulator')

test.compile(make_top_shell=False,
             make_main=False,
             make_pli=True,
             iv_flags2=["-g2005-sv"],
             verilator_flags2=["--exe --timing --vpi", test.pli_filename])

test.execute(use_libvpi=True)

test.passes()
