#include "V3SimpleAssignGraph.h"

#include "V3Ast.h"
#include "V3AstAttr.h"
#include "V3AstNodeExpr.h"
#include "V3Graph.h"

VL_DEFINE_DEBUG_FUNCTIONS;

class V3VarScopeVertex final : public V3GraphVertex {
public:
    std::string name() const override {
        UASSERT_OBJ(userp(), this, "Trying to print null node");
        return reinterpret_cast<AstVarScope*>(userp())->prettyName();
    }
    V3VarScopeVertex(const AstVarScope* nodep, V3Graph* graphp)
        : V3GraphVertex(graphp) {
        userp(reinterpret_cast<void*>(const_cast<AstVarScope*>(nodep)));
    }
};

class SimpleAssignGraphVisitor final : public VNVisitorConst {
    std::unordered_map<AstVarScope*, V3VarScopeVertex*> m_vertices{};
    std::unique_ptr<V3Graph> m_graphp{std::make_unique<V3Graph>()};

    V3VarScopeVertex* getOrCreateVertex(AstVarScope* vscp) {
        const auto it = m_vertices.find(vscp);
        if (it != m_vertices.end()) return it->second;
        V3VarScopeVertex* const vtxp = new V3VarScopeVertex{vscp, m_graphp.get()};
        m_vertices.emplace(vscp, vtxp);
        return vtxp;
    }

    void visit(AstCFunc* nodep) override {
        m_graphp->clear();
        m_vertices.clear();
        iterateChildrenConst(nodep);
        m_graphp->dumpDotFilePrefixed("simple_assign_graph_" + nodep->name());
    }

    void visit(AstAssign* nodep) override {
        const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
        const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);
        if (lhsp && rhsp && lhsp->access().isWriteOnly() && rhsp->access().isReadOnly()) {
            UASSERT_OBJ(lhsp->varScopep(), nodep, "No scope found");
            UASSERT_OBJ(rhsp->varScopep(), nodep, "No scope found");
            V3VarScopeVertex* const lhsVtxp = getOrCreateVertex(lhsp->varScopep());
            V3VarScopeVertex* const rhsVtxp = getOrCreateVertex(rhsp->varScopep());

            new V3GraphEdge(m_graphp.get(), rhsVtxp, lhsVtxp, 1);
        }
    }
    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

public:
    SimpleAssignGraphVisitor(AstNode* nodep) { iterateConst(nodep); }
    ~SimpleAssignGraphVisitor() = default;
};

void V3SimpleAssignGraph::dumpSimpleAssigns(AstNetlist* rootp) { SimpleAssignGraphVisitor{rootp}; }