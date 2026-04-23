#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of either the GNU Lesser General Public License Version 3
# or the Perl Artistic License Version 2.0.
# SPDX-FileCopyrightText: 2024 Wilson Snyder
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios('simulator')

test.lint(verilator_flags2=["--debug --debugi 0 --debugi-V3SelfAssign 9"])

tree_before_selfassign_elim_filename = test.glob_one(test.obj_dir + "/V*_width.tree")
tree_after_selfassign_elim_filename = test.glob_one(test.obj_dir + "/V*_selfAssign.tree")

if test.vlt_all:
    self_assign_lhs_pattern = r"VARREF.*x \[RV] <- VAR.*x \[VSTATICI]  VAR"
    self_assign_rhs_pattern = r"VARREF.*x \[LV] => VAR.*x \[VSTATICI]  VAR"
    normal_assign_lhs_pattern = r"VARREF.*y \[RV] <- VAR.*y \[VSTATICI]  VAR"
    normal_assign_rhs_pattern = r"VARREF.*z \[LV] => VAR.*z \[VSTATICI]  VAR"

    test.file_grep(tree_before_selfassign_elim_filename, self_assign_lhs_pattern)
    test.file_grep(tree_before_selfassign_elim_filename, self_assign_rhs_pattern)
    test.file_grep(tree_before_selfassign_elim_filename, normal_assign_lhs_pattern)
    test.file_grep(tree_before_selfassign_elim_filename, normal_assign_rhs_pattern)

    test.file_grep_not(tree_after_selfassign_elim_filename, self_assign_lhs_pattern)
    test.file_grep_not(tree_after_selfassign_elim_filename, self_assign_rhs_pattern)
    test.file_grep(tree_after_selfassign_elim_filename, normal_assign_lhs_pattern)
    test.file_grep(tree_after_selfassign_elim_filename, normal_assign_rhs_pattern)

test.passes()
