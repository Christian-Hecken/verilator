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

class RefCountVisitor final : public VNVisitor {

    void visit(AstVarRef* nodep) {
        if (AstVar* const varp = nodep->varp()) { varp->user1Inc(); }
        iterateChildren(nodep);
    }

    void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    explicit RefCountVisitor(AstNetlist* nodep) { iterateChildren(nodep); }
    ~RefCountVisitor() override = default;
};

//######################################################################
// RefCount printer

class RefCountPrinter final : public VNVisitorConst {
    void visit(AstVar* nodep) {
        V3Stats::addStatSum("Signal " + nodep->prettyName() + " usages: ", nodep->user1());
        iterateChildrenConst(nodep);
    }
    void visit(AstNode* nodep) { iterateChildrenConst(nodep); }

public:
    explicit RefCountPrinter(AstNetlist* nodep) { iterateChildrenConst(nodep); }
    ~RefCountPrinter() override = default;
};

//######################################################################
// RefCount class functions

void V3RefCount::countAll(AstNetlist* nodep) {
    {
        const VNUser1InUse m_inuser1;
        RefCountVisitor{nodep};
        RefCountPrinter{nodep};
    }
}
