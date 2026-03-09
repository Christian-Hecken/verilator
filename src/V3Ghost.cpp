// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Ghost variable analysis
//
// Identifies public signals that can be safely optimized away from the
// hot evaluation path and lazily reconstructed on external access.
//
// A signal is ghost-eligible if:
//   1. It is marked read-only public (public_flat_rd) but NOT read-write
//   2. It has exactly one combinational (non-clocked) driver
//   3. It is not a primary I/O
//   4. It is not forced or written by DPI
//   5. The driver expression's inputs are all surviving signals (not themselves ghosts)
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

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3Ghost.h"

#include "V3Stats.h"

#include <unordered_map>
#include <unordered_set>

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// Ghost eligibility analysis

class GhostVisitor final : public VNVisitorConst {
    // NODE STATE
    // AstVarScope::user1  -> int: number of combinational write refs
    // AstVarScope::user2  -> int: number of clocked/procedural write refs
    const VNUser1InUse m_user1InUse;
    const VNUser2InUse m_user2InUse;

    // STATE
    bool m_inClockedActive = false;  // Currently under a clocked active block
    VDouble0 m_statGhosts;  // Statistic tracking

    // Map from VarScope to the RHS expression of its continuous assign driver.
    // Only populated for signals with a single combinational driver.
    std::unordered_map<const AstVarScope*, AstNodeExpr*> m_driverExpr;

    // METHODS

    // Check whether all VarRef reads in an expression reference surviving (non-ghost) signals
    static bool allInputsSurvive(const AstNode* nodep) {
        // Walk the expression tree; if any read-reference points to a ghost var, return false
        if (const AstNodeVarRef* const vrefp = VN_CAST(nodep, NodeVarRef)) {
            if (vrefp->access().isReadOrRW()) {
                const AstVar* const varp = vrefp->varp();
                if (varp->isGhost()) return false;
            }
        }
        // Recurse into children
        for (const AstNode* childp = nodep->op1p(); childp; childp = childp->nextp()) {
            if (!allInputsSurvive(childp)) return false;
        }
        for (const AstNode* childp = nodep->op2p(); childp; childp = childp->nextp()) {
            if (!allInputsSurvive(childp)) return false;
        }
        for (const AstNode* childp = nodep->op3p(); childp; childp = childp->nextp()) {
            if (!allInputsSurvive(childp)) return false;
        }
        for (const AstNode* childp = nodep->op4p(); childp; childp = childp->nextp()) {
            if (!allInputsSurvive(childp)) return false;
        }
        return true;
    }

    // VISITORS
    void visit(AstActive* nodep) override {
        VL_RESTORER(m_inClockedActive);
        m_inClockedActive = nodep->hasClocked();
        iterateChildrenConst(nodep);
    }

    void visit(AstAssignW* nodep) override {
        // Continuous assignment — always combinational
        if (AstVarRef* const lhsVarRefp = VN_CAST(nodep->lhsp(), VarRef)) {
            if (lhsVarRefp->access().isWriteOrRW()) {
                AstVarScope* const vscp = lhsVarRefp->varScopep();
                // Count combinational writes
                vscp->user1(vscp->user1() + 1);
                // Record driver expression (overwrite if multiple — we'll check count later)
                m_driverExpr[vscp] = nodep->rhsp();
            }
        }
        iterateChildrenConst(nodep);
    }

    void visit(AstAssign* nodep) override {
        // Procedural assignment — may be clocked or combinational
        if (AstVarRef* const lhsVarRefp = VN_CAST(nodep->lhsp(), VarRef)) {
            if (lhsVarRefp->access().isWriteOrRW()) {
                AstVarScope* const vscp = lhsVarRefp->varScopep();
                if (m_inClockedActive) {
                    // Clocked write — disqualifies from ghosting
                    vscp->user2(vscp->user2() + 1);
                } else {
                    // Combinational procedural write
                    vscp->user1(vscp->user1() + 1);
                    m_driverExpr[vscp] = nodep->rhsp();
                }
            }
        }
        iterateChildrenConst(nodep);
    }

    void visit(AstAssignDly* nodep) override {
        // Non-blocking assignment — always clocked (disqualifies from ghosting)
        if (AstVarRef* const lhsVarRefp = VN_CAST(nodep->lhsp(), VarRef)) {
            if (lhsVarRefp->access().isWriteOrRW()) {
                AstVarScope* const vscp = lhsVarRefp->varScopep();
                vscp->user2(vscp->user2() + 1);
            }
        }
        iterateChildrenConst(nodep);
    }

    void visit(AstNodeVarRef* nodep) override {
        // Only interested in write refs under assignments, handled above
    }

    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

    // CONSTRUCTOR
    explicit GhostVisitor(AstNetlist* netlistp) {
        // Pass 1: Walk the entire design to count writers and record driver expressions
        iterateChildrenConst(netlistp);

        // Pass 2: Mark eligible signals as ghosts
        // Walk all VarScope nodes and check eligibility
        netlistp->foreach([this](AstVarScope* vscp) {
            AstVar* const varp = vscp->varp();

            // Only consider read-only public signals (not read-write)
            if (!varp->isSigUserRdPublic()) return;
            if (varp->isSigUserRWPublic()) return;

            UINFO(5, "Ghost candidate: "
                         << varp->prettyNameQ() << " primaryIO=" << varp->isPrimaryIO()
                         << " forced=" << varp->isForced() << " param=" << varp->isParam()
                         << " comboW=" << vscp->user1() << " clockW=" << vscp->user2() << endl);

            // Skip I/O ports — submodule ports retain VVarType::PORT after inlining
            // even though their direction is cleared. DPI context code accesses these
            // via raw data pointers that bypass ghost callbacks.
            if (varp->isPrimaryIO()) return;
            if (varp->varType() == VVarType::PORT) return;

            // Skip forced or DPI-written signals
            if (varp->isForced()) return;
            if (varp->isForceable()) return;
            if (varp->isWrittenByDpi()) return;

            // Skip parameters
            if (varp->isParam()) return;

            // Must have exactly one combinational driver and no clocked drivers
            const int comboWrites = vscp->user1();
            const int clockedWrites = vscp->user2();
            if (comboWrites != 1 || clockedWrites != 0) return;

            // Must have a recorded driver expression
            const auto it = m_driverExpr.find(vscp);
            if (it == m_driverExpr.end()) return;

            // All inputs of the driver expression must be surviving signals
            // (In the initial implementation, we don't handle transitive ghosts)
            if (!allInputsSurvive(it->second)) return;

            // This signal is ghost-eligible!
            UINFO(4, "Ghost-eligible: " << varp->prettyNameQ() << endl);
            varp->setGhost();
            ++m_statGhosts;
        });
    }

public:
    ~GhostVisitor() override { V3Stats::addStat("Optimizations, Ghost variables", m_statGhosts); }
    static void apply(AstNetlist* netlistp) { GhostVisitor{netlistp}; }
};

//######################################################################
// V3Ghost class functions

void V3Ghost::ghostAll(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ": " << endl);
    GhostVisitor::apply(nodep);
    V3Global::dumpCheckGlobalTree("ghost", 0, dumpTreeEitherLevel() >= 3);
}
