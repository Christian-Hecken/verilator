// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Create ghost variable lazy-eval callback functions
//
// After V3Gate has inlined ghost expressions into RTL consumers, this pass:
//   1. Finds ghost variable assignments remaining in Active blocks
//   2. Determines which ghosts can be removed from the eval loop (pinning analysis)
//   3. Creates a single ghost-eval AstCFunc per scope with all un-pinned ghost
//      assignments in topological (dependency) order
//   4. Removes un-pinned ghost assignments from the Active blocks
//
// The ghost-eval function is later registered as a VPI read callback
// by V3EmitCSyms, so ghost variables are only computed on external access.
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

#include "V3GhostFunc.h"

#include "V3EmitCBase.h"
#include "V3Stats.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// Ghost function creation

class GhostFuncVisitor final : public VNVisitorConst {
    // TYPES
    struct GhostInfo {
        AstVarScope* vscp;  // The ghost variable scope
        AstNodeAssign* assignp;  // Its assignment statement
        AstScope* scopep;  // Scope containing the assignment
    };

    // STATE
    VDouble0 m_statGhostFuncs;  // Count of ghost variables moved to callbacks

    // Collected ghost assignments, keyed by VarScope
    std::unordered_map<const AstVarScope*, GhostInfo> m_ghostAssigns;

    // For each ghost VarScope, which other ghost VarScopes does its expression read?
    std::unordered_map<const AstVarScope*, std::unordered_set<const AstVarScope*>>
        m_ghostReadsFrom;

    // Ghost VarScopes that have at least one eval-loop (non-ghost) reader
    std::unordered_set<const AstVarScope*> m_evalReaders;

    // Current assignment LHS VarScope (while walking an assignment RHS)
    const AstVarScope* m_curAssignLhsVscp = nullptr;
    bool m_curAssignIsGhostDriver = false;

    // METHODS

    // Collect ghost variable assignments from Active blocks
    void collectGhostAssigns(AstNetlist* netlistp) {
        // Walk all AstActive blocks under AstTopScope, find ghost assigns
        netlistp->foreach([this](AstAssignW* nodep) {
            if (AstVarRef* const lhsp = VN_CAST(nodep->lhsp(), VarRef)) {
                if (lhsp->access().isWriteOrRW() && lhsp->varp()->isGhost()) {
                    AstVarScope* const vscp = lhsp->varScopep();
                    AstScope* const scopep = vscp->scopep();
                    m_ghostAssigns[vscp] = {vscp, nodep, scopep};
                    UINFO(5, "Ghost assign found: " << vscp->prettyNameQ() << endl);
                }
            }
        });
    }

    // Build the ghost-reads-from graph and find eval-loop readers
    void analyzeReaders(AstNetlist* netlistp) {
        // Walk ALL varref nodes in the design
        netlistp->foreach([this](const AstVarRef* vrefp) {
            if (!vrefp->access().isReadOrRW()) return;
            AstVarScope* const readVscp = vrefp->varScopep();
            if (!readVscp->varp()->isGhost()) return;
            // This is a read of a ghost variable.
            // Determine context: is this read inside a ghost assignment's RHS?
            // Walk up to find the enclosing AstAssignW/AstAssign
            const AstNode* nodep = vrefp;
            const AstNodeAssign* enclosingAssign = nullptr;
            while (nodep) {
                enclosingAssign = VN_CAST(nodep, NodeAssign);
                if (enclosingAssign) break;
                nodep = nodep->backp();
            }
            if (enclosingAssign) {
                // Check if the enclosing assignment's LHS is a ghost variable
                if (const AstVarRef* const lhsp = VN_CAST(enclosingAssign->lhsp(), VarRef)) {
                    AstVarScope* const writerVscp = lhsp->varScopep();
                    if (writerVscp->varp()->isGhost() && m_ghostAssigns.count(writerVscp)) {
                        // Ghost-to-ghost reference: readVscp is read by writerVscp's expr
                        m_ghostReadsFrom[writerVscp].insert(readVscp);
                        return;  // Not an eval-loop reader
                    }
                }
            }
            // It's an eval-loop (non-ghost) read of this ghost variable
            UINFO(5, "Ghost eval-loop reader: " << readVscp->prettyNameQ()
                                                << " (from non-ghost context)" << endl);
            m_evalReaders.insert(readVscp);
        });
    }

    // Iterative pinning: determine which ghosts must stay in the eval loop
    std::unordered_set<const AstVarScope*> computePinned() {
        std::unordered_set<const AstVarScope*> pinned = m_evalReaders;
        std::unordered_set<const AstVarScope*> unpinned;
        for (const auto& pair : m_ghostAssigns) {
            if (!pinned.count(pair.first)) unpinned.insert(pair.first);
        }

        // Build reverse map: for each ghost G, which ghosts read from G?
        std::unordered_map<const AstVarScope*, std::unordered_set<const AstVarScope*>> readersOf;
        for (const auto& pair : m_ghostReadsFrom) {
            for (const AstVarScope* src : pair.second) { readersOf[src].insert(pair.first); }
        }

        // Iterative pinning
        bool changed = true;
        while (changed) {
            changed = false;
            std::unordered_set<const AstVarScope*> newlyPinned;
            for (const AstVarScope* vscp : unpinned) {
                // Check if any reader of this ghost is pinned
                const auto it = readersOf.find(vscp);
                if (it != readersOf.end()) {
                    for (const AstVarScope* reader : it->second) {
                        if (pinned.count(reader)) {
                            newlyPinned.insert(vscp);
                            break;
                        }
                    }
                }
            }
            if (!newlyPinned.empty()) {
                changed = true;
                for (const AstVarScope* vscp : newlyPinned) {
                    pinned.insert(vscp);
                    unpinned.erase(vscp);
                    UINFO(5, "Ghost pinned (transitive): " << vscp->prettyNameQ() << endl);
                }
            }
        }

        return pinned;
    }

    // Topologically sort un-pinned ghost assignments so dependencies come first
    std::vector<const AstVarScope*>
    topoSort(const std::unordered_set<const AstVarScope*>& unpinned) {
        // Build adjacency: for each unpinned ghost, which unpinned ghosts does it depend on?
        std::unordered_map<const AstVarScope*, std::unordered_set<const AstVarScope*>> deps;
        std::unordered_map<const AstVarScope*, int> inDegree;
        for (const AstVarScope* vscp : unpinned) {
            inDegree[vscp] = 0;
            deps[vscp];  // ensure entry exists
        }
        for (const AstVarScope* vscp : unpinned) {
            const auto it = m_ghostReadsFrom.find(vscp);
            if (it != m_ghostReadsFrom.end()) {
                for (const AstVarScope* dep : it->second) {
                    if (unpinned.count(dep)) {
                        deps[vscp].insert(dep);
                        ++inDegree[dep];  // dep is depended-upon by vscp
                    }
                }
            }
        }

        // Wait, the direction is: vscp's expression reads from dep.
        // So dep must be evaluated before vscp. Topological order: dep before vscp.
        // inDegree counts how many unpinned ghosts READ from this ghost.
        // Ghosts with inDegree 0 are leaves (no one depends on them)... that's backwards.
        // Let me redo:

        // Correct direction: if vscp's expression reads dep, then dep -> vscp in the DAG.
        // We want topological order where dep comes before vscp.
        std::unordered_map<const AstVarScope*, int> inDeg;
        for (const AstVarScope* vscp : unpinned) inDeg[vscp] = 0;
        for (const AstVarScope* vscp : unpinned) {
            const auto it = m_ghostReadsFrom.find(vscp);
            if (it != m_ghostReadsFrom.end()) {
                for (const AstVarScope* dep : it->second) {
                    if (unpinned.count(dep)) {
                        ++inDeg[vscp];  // vscp depends on dep
                    }
                }
            }
        }

        std::vector<const AstVarScope*> order;
        std::vector<const AstVarScope*> queue;
        for (const auto& pair : inDeg) {
            if (pair.second == 0) queue.push_back(pair.first);
        }
        while (!queue.empty()) {
            const AstVarScope* const cur = queue.back();
            queue.pop_back();
            order.push_back(cur);
            // For all ghosts that read from cur, decrement inDegree
            for (const AstVarScope* vscp : unpinned) {
                const auto it = m_ghostReadsFrom.find(vscp);
                if (it != m_ghostReadsFrom.end() && it->second.count(cur)) {
                    if (--inDeg[vscp] == 0) queue.push_back(vscp);
                }
            }
        }

        UASSERT(order.size() == unpinned.size(), "Ghost topological sort failed — cycle detected");
        return order;
    }

    // Create one ghost eval CFunc per scope and remove assignments from Active blocks
    void createGhostFuncs(AstNetlist* netlistp, const std::vector<const AstVarScope*>& order) {
        if (order.empty()) return;

        // Group un-pinned ghosts by scope (preserving topological order within each group)
        std::unordered_map<AstScope*, std::vector<const AstVarScope*>> scopeGroups;
        for (const AstVarScope* vscp : order) {
            AstScope* const scopep = m_ghostAssigns.at(vscp).scopep;
            scopeGroups[scopep].push_back(vscp);
        }

        // Create one CFunc per scope
        for (auto& pair : scopeGroups) {
            AstScope* const scopep = pair.first;
            const auto& group = pair.second;
            AstNodeModule* const modp = scopep->modp();
            FileLine* const fl = modp->fileline();

            // Create a static CFunc with void* argument (matches VlGhostReadCb signature)
            AstCFunc* const funcp = new AstCFunc{fl, "_ghostEval", scopep, ""};
            funcp->isStatic(true);
            funcp->isLoose(true);
            funcp->declPrivate(true);
            funcp->slow(true);  // Ghost eval is cold path (VPI reads only)
            funcp->protect(false);  // Keep name predictable for V3EmitCSyms
            funcp->dontCombine(true);
            funcp->argTypes("void* voidSelf");

            // Add voidSelf -> vlSelf cast
            funcp->addStmtsp(new AstCStmt{fl, EmitCUtil::voidSelfAssign(modp)});
            funcp->addStmtsp(new AstCStmt{fl, EmitCUtil::symClassAssign()});

            // Move ghost assignments into the CFunc in topological order
            for (const AstVarScope* vscp : group) {
                GhostInfo& info = m_ghostAssigns.at(vscp);
                AstNodeAssign* const assignp = info.assignp;
                UINFO(4, "Ghost moved to callback: " << vscp->prettyNameQ() << endl);
                assignp->unlinkFrBack();
                funcp->addStmtsp(assignp);
                ++m_statGhostFuncs;
            }

            // Attach CFunc to the scope
            scopep->addBlocksp(funcp);

            UINFO(2, "Created ghost eval CFunc for " << modp->prettyNameQ() << " with "
                                                     << group.size() << " assignments" << endl);
        }
    }

    // VISITORS
    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

    // CONSTRUCTOR
    explicit GhostFuncVisitor(AstNetlist* netlistp) {
        // Step 1: Collect ghost variable assignments
        collectGhostAssigns(netlistp);
        if (m_ghostAssigns.empty()) return;

        // Step 2: Analyze readers to build ghost dependency graph and find eval-loop readers
        analyzeReaders(netlistp);

        // Step 3: Iterative pinning — determine which ghosts must stay in eval loop
        const auto pinned = computePinned();

        // Step 4: Clear ghost flag on pinned variables so V3EmitCSyms
        // won't register callbacks for them (they stay in the eval loop)
        for (const AstVarScope* vscp : pinned) {
            GhostInfo& info = m_ghostAssigns.at(vscp);
            info.vscp->varp()->clearGhost();
            UINFO(4, "Ghost pinned (cleared): " << vscp->prettyNameQ() << endl);
        }

        // Step 5: Collect un-pinned ghosts
        std::unordered_set<const AstVarScope*> unpinned;
        for (const auto& pair : m_ghostAssigns) {
            if (!pinned.count(pair.first)) {
                unpinned.insert(pair.first);
                UINFO(4, "Ghost un-pinned (lazy): " << pair.first->prettyNameQ() << endl);
            }
        }

        if (unpinned.empty()) {
            UINFO(2, "All ghost variables are pinned; no lazy eval possible" << endl);
            return;
        }

        // Step 5: Topological sort of un-pinned ghosts
        const auto order = topoSort(unpinned);

        // Step 6: Create CFunc(s) and remove assignments from Active blocks
        createGhostFuncs(netlistp, order);
    }

public:
    ~GhostFuncVisitor() override {
        V3Stats::addStat("Optimizations, Ghost variables lazy-eval", m_statGhostFuncs);
    }
    static void apply(AstNetlist* netlistp) { GhostFuncVisitor{netlistp}; }
};

//######################################################################
// V3GhostFunc class functions

void V3GhostFunc::ghostFuncAll(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ": " << endl);
    GhostFuncVisitor::apply(nodep);
    V3Global::dumpCheckGlobalTree("ghostfunc", 0, dumpTreeEitherLevel() >= 3);
}
