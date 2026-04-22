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
    const VNUser1InUse m_inuser1;

    // VISITORS
    void visit(AstVarRef* nodep) {
        UINFO(6, "Encountered signal '"
                     << nodep->varp()->prettyName() << "' usage, incrementing user1 from "
                     << nodep->varp()->user1() << " to " << (nodep->varp()->user1() + 1));
        nodep->varp()->user1Inc();
        iterateChildrenConst(nodep);
    }

    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

public:
    // CONSTRUCTORS
    explicit RefCountVisitor(AstNetlist* nodep) {
        AstNode::user1ClearTree();
        iterateChildrenConst(nodep);
    }
    ~RefCountVisitor() override = default;
};

//######################################################################
// Print visitor

class printRefCountVisitor final : public VNVisitorConst {
    const VNUser1InUse m_inuser1;
    void visit(AstVar* nodep) override {
        V3Stats::addStat("Signal " + nodep->prettyName() + " usages: ",
                         static_cast<double>(nodep->user1()));
        // V3Stats::addStatSum("Signal " + nodep->prettyName() + " usages: ", nodep->user1());
        iterateChildrenConst(nodep);
    }

    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

public:
    explicit printRefCountVisitor(AstNetlist* nodep) { iterateChildrenConst(nodep); }
    ~printRefCountVisitor() override = default;
};

//######################################################################
// RefCount class functions

void V3RefCount::countAll(AstNetlist* nodep) {
    {
        RefCountVisitor{nodep};
        printRefCountVisitor{nodep};
    }
}
