#!/usr/bin/env python3
# DESCRIPTION: Verilator: DPI/VPI immediate-eval repro test runner
# SPDX-FileCopyrightText: 2026 Example
# SPDX-License-Identifier: CC0-1.0

import vltest_bootstrap

test.scenarios('simulator')

test.compile(make_top_shell=False,
             make_main=False,
             make_pli=True,
             verilator_flags2=["+define+ENABLE_EVAL --binary --vpi", test.pli_filename],
             v_flags2=["+define+USE_VPI_NOT_DPI"])

test.execute(xrun_flags2=["+define+USE_VPI_NOT_DPI"], use_libvpi=True, check_finished=True)

test.passes()
