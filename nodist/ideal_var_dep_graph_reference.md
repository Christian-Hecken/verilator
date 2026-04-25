# Reference Implementation: V3VarDepGraph

This is a patch-style reference implementation for the "baby graph pass" exercise.
It keeps the pass read-only and dumps variable dependency graphs for simple direct assignments.

## New file: src/V3VarDepGraph.h

```cpp
// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Debug variable dependency graph dumping
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

#ifndef VERILATOR_V3VARDEPGRAPH_H_
#define VERILATOR_V3VARDEPGRAPH_H_

#include "config_build.h"
#include "verilatedos.h"

class AstNetlist;

class V3VarDepGraph final {
public:
    static void dumpGraphAll(AstNetlist* nodep) VL_MT_DISABLED;
};

#endif  // Guard
```

## New file: src/V3VarDepGraph.cpp

```cpp
// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Debug variable dependency graph dumping
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

#include "V3VarDepGraph.h"

#include "V3Graph.h"

#include <unordered_map>

VL_DEFINE_DEBUG_FUNCTIONS;

class VarDepVertex final : public V3GraphVertex {
    VL_RTTI_IMPL(VarDepVertex, V3GraphVertex)
    AstVarScope* const m_vscp;

public:
    VarDepVertex(V3Graph* graphp, AstVarScope* vscp)
        : V3GraphVertex{graphp}
        , m_vscp{vscp} {}
    ~VarDepVertex() override = default;

    AstVarScope* vscp() const { return m_vscp; }

    string name() const override VL_MT_STABLE {
        return cvtToHex(vscp()) + " " + vscp()->name();
    }

    string dotShape() const override { return "ellipse"; }

    string dotColor() const override {
        const AstVar* const varp = vscp()->varp();
        if (varp->isPrimaryIO()) return "blue";
        if (varp->isTemp()) return "gray";
        return "black";
    }
};

class VarDepEdge final : public V3GraphEdge {
    VL_RTTI_IMPL(VarDepEdge, V3GraphEdge)

public:
    VarDepEdge(V3Graph* graphp, VarDepVertex* fromp, VarDepVertex* top)
        : V3GraphEdge{graphp, fromp, top, 1, false} {}
    ~VarDepEdge() override = default;

    string dotColor() const override { return "darkgreen"; }
};

class VarDepGraphVisitor final : public VNVisitor {
    // STATE
    AstCFunc* m_funcp = nullptr;  // Current C function being analyzed
    V3Graph m_graph;
    std::unordered_map<AstVarScope*, VarDepVertex*> m_varVertices;
    uint32_t m_graphIdx = 0;

    // METHODS
    VarDepVertex* getVertex(AstVarScope* vscp) {
        auto it = m_varVertices.find(vscp);
        if (it != m_varVertices.end()) return it->second;
        VarDepVertex* const vtxp = new VarDepVertex{&m_graph, vscp};
        m_varVertices.emplace(vscp, vtxp);
        return vtxp;
    }

    void dumpCurrentGraph() {
        if (m_graph.empty()) return;
        const string name = "vardep_" + cvtToStr(++m_graphIdx) + "_" + m_funcp->name();
        m_graph.dumpDotFilePrefixed(name);
    }

    // VISITORS
    void visit(AstCFunc* nodep) override {
        if (dumpGraphLevel() < 9) return;

        VL_RESTORER(m_funcp);
        m_funcp = nodep;

        m_graph.clear();
        m_varVertices.clear();

        iterateChildren(nodep);
        dumpCurrentGraph();
    }

    void visit(AstNodeAssign* nodep) override {
        if (!m_funcp) return;

        // Keep the exercise deliberately scoped: only direct var-to-var copies.
        if (nodep->timingControlp()) return;

        const AstVarRef* const lhsp = VN_CAST(nodep->lhsp(), VarRef);
        const AstVarRef* const rhsp = VN_CAST(nodep->rhsp(), VarRef);
        if (!lhsp || !rhsp) return;

        if (!lhsp->access().isWriteOrRW()) return;
        if (!rhsp->access().isReadOrRW()) return;

        AstVarScope* const lhsVscp = lhsp->varScopep();
        AstVarScope* const rhsVscp = rhsp->varScopep();
        if (!lhsVscp || !rhsVscp) return;
        if (lhsVscp == rhsVscp) return;

        new VarDepEdge{&m_graph, getVertex(rhsVscp), getVertex(lhsVscp)};
    }

    void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    explicit VarDepGraphVisitor(AstNetlist* nodep) {
        if (dumpGraphLevel() >= 9) iterate(nodep);
    }
    ~VarDepGraphVisitor() override = default;
};

void V3VarDepGraph::dumpGraphAll(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ":");
    { VarDepGraphVisitor{nodep}; }
}
```

## Update: src/Verilator.cpp

```diff
@@
 #include "V3VariableOrder.h"
+#include "V3VarDepGraph.h"
 #include "V3Waiver.h"
@@
             // Add C casts when longs need to become long-long and vice-versa
             // Note depth may insert something needing a cast, so this must be last.
             V3Cast::castAll(v3Global.rootp());
+
+            // Debug-only graph exercise pass: dump simple var dependency edges.
+            if (dumpGraphLevel() >= 9) V3VarDepGraph::dumpGraphAll(v3Global.rootp());
         }
```

## Update: src/CMakeLists.txt

```diff
@@
     V3VariableOrder.h
+    V3VarDepGraph.h
     V3Waiver.h
@@
     V3VariableOrder.cpp
+    V3VarDepGraph.cpp
     V3Waiver.cpp
```

## Update: src/Makefile_obj.in

```diff
@@
   V3VariableOrder.o \
+  V3VarDepGraph.o \
   V3Waiver.o \
```
