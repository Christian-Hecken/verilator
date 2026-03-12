#!/usr/bin/env python3
import vltest_bootstrap

test.scenarios('simulator')
# use the same C++ PLI file as the original noncontinuous-release test
test.pli_filename = "t/t_vpi_noncontinuous_release.cpp"

# same build flags as the original test; the C++ helper provides sv_check()

test.compile(make_top_shell=False,
             make_main=False,
             make_pli=True,
             verilator_flags2=["--binary --vpi", test.pli_filename])

test.execute(use_libvpi=True)

test.passes()
