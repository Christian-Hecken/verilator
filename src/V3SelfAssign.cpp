#include "V3SelfAssign.h"

#include "V3Ast.h"

VL_DEFINE_DEBUG_FUNCTIONS;

class V3selfAssignVisitor final : public VNVisitor {

    void visit(AstAssign* nodep) override {
        const AstVarRef* const lhsp = VN_CAST(nodep->lhsp(), VarRef);
        const AstVarRef* const rhsp = VN_CAST(nodep->rhsp(), VarRef);
        if (rhsp && lhsp && rhsp->varScopep() == lhsp->varScopep() && rhsp->varp() == lhsp->varp()
            && !nodep->isTimingControl())
            nodep->user1SetOnce();
    }
    void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    V3selfAssignVisitor(AstNetlist* nodep) { iterate(nodep); }
    ~V3selfAssignVisitor() = default;
};

class V3SelfAssignDeleter final : public VNVisitor {
    void visit(AstNode* nodep) override {
        if (nodep && nodep->user1())
            pushDeletep(nodep->unlinkFrBack());
        else
            iterateChildren(nodep);
    }

public:
    V3SelfAssignDeleter(AstNetlist* nodep) { iterate(nodep); }
    ~V3SelfAssignDeleter() = default;
};

void V3SelfAssign::deleteSelfAssigns(AstNetlist* nodep) {
    VNUser1InUse m_userres;
    AstNode::user1ClearTree();
    V3selfAssignVisitor selfAssignVisitor(nodep);
    V3SelfAssignDeleter selfAssignDeleter(nodep);
    V3Global::dumpCheckGlobalTree("selfAssign", 0, dumpTreeEitherLevel() >= 3);
}