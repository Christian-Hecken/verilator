#include "V3Alias.h"

#include "V3Ast.h"

bool isAliasingAssigmnent(const AstAssign* nodep) {
    const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
    const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);
    // Do NOT check for public access - treat all aliases the same to allow for optimization

    return !nodep->isTimingControl() && lhsp && rhsp && !lhsp->isTimingControl()
           && !rhsp->isTimingControl() && lhsp->access().isWriteOnly()
           && rhsp->access().isReadOnly();
}

class AliasDetectionVisitor : public VNVisitorConst {
    using Driver = const AstVar*;
    using Alias = const AstVar*;
    std::unordered_map<Alias, Driver> m_aliases;
    std::unordered_set<Alias> m_multiDrivenVars;
    void visit(AstAssign* nodep) override {
        const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
        const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);
        const AstVar* lhsVarp = lhsp->varp();
        const AstVar* rhsVarp = rhsp->varp();
        UASSERT_OBJ(lhsVarp, lhsp, "Assignment lhs var reference has no associated AstVar");
        UASSERT_OBJ(rhsVarp, rhsp, "Assignment rhs var reference has no associated AstVar");
        if (isAliasingAssigmnent(nodep)
            && m_multiDrivenVars.find(lhsVarp) == m_multiDrivenVars.end()) {
            Driver currentDriver = rhsVarp;
            Driver previousDriver = [this, lhsVarp]() {
                auto it = m_aliases.find(lhsVarp);
                return (it != m_aliases.end()) ? it->second : nullptr;
            }();
            if (previousDriver && currentDriver != previousDriver)
                m_multiDrivenVars.insert(lhsVarp);
            else
                m_aliases[lhsVarp] = currentDriver;
        }
    }
    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

public:
    AliasDetectionVisitor(AstNetlist* rootp) { iterateConst(rootp); }
    static std::unordered_set<const AstVar*> findAliasingVars(AstNetlist* rootp) {
        auto aliasesAndDrivers = AliasDetectionVisitor{rootp}.m_aliases;
        std::unordered_set<Alias> aliases;
        for (const std::pair<Alias, Driver> aliasAndDriver : aliasesAndDrivers) {
            aliases.insert(aliasAndDriver.first);
        }
        return aliases;
    }
};

class AliasReplacementVisitor : public VNVisitor {
    const std::unordered_set<const AstVar*> m_aliasingVars;
    void visit(AstAssign* nodep) override {
        const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
        const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);
        const AstVar* lhsVarp = lhsp->varp();
        const AstVar* rhsVarp = rhsp->varp();
        UASSERT_OBJ(lhsVarp, lhsp, "Assignment lhs var reference has no associated AstVar");
        UASSERT_OBJ(rhsVarp, rhsp, "Assignment rhs var reference has no associated AstVar");
        if (isAliasingAssigmnent(nodep)
            && m_aliasingVars.find(lhsVarp) != m_aliasingVars.end()) {
                // TODO: Need to find the driver's varScope(?)
                // Then replace the aliasee's varScope with the driver's varScope
                // Ah, but difficulty: This must be done for _every_ occurrence of the aliasee's varScope, not just within assignments!
                // Finally, remove the AstVar* for the aliasee itself
        }
    }
    void visit(AstNode* nodep) override { iterateChildren(nodep); }
public:
    AliasReplacementVisitor(const std::unordered_set<const AstVar*> aliasingVars) : m_aliasingVars(aliasingVars) {}
    static void
    replacePublicAliasesWithDrivers(const AstNetlist* rootp,
                                    const std::unordered_set<const AstVar*>& aliasingVars) {}
};

void V3Alias::removePublicAliases(AstNetlist* rootp) {
    const std::unordered_set<const AstVar*> aliasingVars
        = AliasDetectionVisitor::findAliasingVars(rootp);
    AliasReplacementVisitor::replacePublicAliasesWithDrivers(rootp, aliasingVars);
}