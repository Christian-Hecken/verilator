// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Variable usage counting
//
// Code available from: https://verilator.org
//
//*************************************************************************
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of either the GNU Lesser General Public License Version 3
// or the Perl Artistic License Version 2.0.
// SPDX-FileCopyrightText: 2003-2026 Wilson Snyder
// SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0
//
//*************************************************************************
// VARIABLE USAGE COUNTING:
//      Count amount of times each variable is used
//
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3RefCount.h"

#include "V3Stats.h"

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// RefCount state, as a visitor of each AstNode

class RefCountVisitor final : public VNVisitorConst {
    std::unordered_map<AstVar*, uint32_t> m_varRefCnts;  // Ref count for each variable

    void visit(AstVarRef* nodep) {
        ++m_varRefCnts[nodep->varp()];
        iterateChildrenConst(nodep);
    }

    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

public:
    explicit RefCountVisitor(AstNetlist* nodep) {
        iterateChildrenConst(nodep);
        for (const std::pair<AstVar*, uint32_t> VarAndCount : m_varRefCnts) {
            const AstVar* var = VarAndCount.first;
            const uint32_t count = VarAndCount.second;
            V3Stats::addStatSum("Signal " + var->prettyName() + " usages: ", count);
        }
    }
    ~RefCountVisitor() override = default;
};

//######################################################################
// RefCount class functions

void V3RefCount::countAll(AstNetlist* nodep) {
    { RefCountVisitor{nodep}; }
}
