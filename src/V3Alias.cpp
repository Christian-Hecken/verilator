#include "V3Alias.h"

#include "V3Ast.h"

using Driver = AstVarScope*;
using Alias = AstVarScope*;

bool isAliasingAssigmnent(const AstAssign* nodep) {
    const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
    const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);
    // Do NOT check for public access - treat all aliases the same to allow for optimization

    return !nodep->isTimingControl() && lhsp && rhsp && !lhsp->isTimingControl()
           && !rhsp->isTimingControl() && lhsp->access().isWriteOnly()
           && rhsp->access().isReadOnly();
}

void checkNoChildren(const AstNode* nodep)
{
    UASSERT_OBJ(!nodep->op1p(), nodep, "Unexpected child under expected leaf node");
    UASSERT_OBJ(!nodep->op2p(), nodep, "Unexpected child under expected leaf node");
    UASSERT_OBJ(!nodep->op3p(), nodep, "Unexpected child under expected leaf node");
    UASSERT_OBJ(!nodep->op4p(), nodep, "Unexpected child under expected leaf node");
}

class AliasDetectionVisitor : public VNVisitorConst {
    std::unordered_map<Alias, Driver> m_aliases;
    std::unordered_set<Alias> m_multiDrivenVars;
    void visit(AstAssign* nodep) override {
        const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
        const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);
        UASSERT_OBJ(lhsp->varScopep(), lhsp,
                    "Assignment lhs var reference has no associated AstVarScope");
        UASSERT_OBJ(rhsp->varScopep(), rhsp,
                    "Assignment rhs var reference has no associated AstVarScope");
        Alias aliasingCandidate = lhsp->varScopep();
        Driver currentDriver = rhsp->varScopep();
        if (isAliasingAssigmnent(nodep)
            && m_multiDrivenVars.find(aliasingCandidate) == m_multiDrivenVars.end()) {
            Driver previousDriver = [this, aliasingCandidate]() {
                auto it = m_aliases.find(aliasingCandidate);
                return (it != m_aliases.end()) ? it->second : nullptr;
            }();
            if (previousDriver && currentDriver != previousDriver)
                m_multiDrivenVars.insert(aliasingCandidate);
            else
                m_aliases[aliasingCandidate] = currentDriver;
        }
    }
    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

public:
    AliasDetectionVisitor(AstNetlist* rootp) { iterateConst(rootp); }
    static std::unordered_map<Alias, Driver> findAliases(AstNetlist* rootp) {
        return AliasDetectionVisitor{rootp}.m_aliases;
    }
};

class AliasReplacementVisitor : public VNVisitor {
    const std::unordered_map<Alias, Driver> m_aliases;
    std::unordered_set<AstVar*> m_aliasVarps;
    void visit(AstVarRef* nodep) override {
        UASSERT_OBJ(nodep->varScopep(), nodep, "AstVarRef has no scope");
        Alias aliasingCandidate = nodep->varScopep();
        if (m_aliases.find(aliasingCandidate) != m_aliases.end()) {
            // AstVarRef* driverp
            //     = new AstVarRef(nullptr, m_aliases.at(aliasingCandidate), VAccess{});
            // nodep->replaceWith(driverp);
            // pushDeletep(nodep);
        }
        checkNoChildren(nodep);
    }
    void visit(AstVarScope* nodep) {
        if (m_aliases.find(nodep) != m_aliases.end()) {
            // Driver driverp = m_aliases.at(nodep);
            // nodep->replaceWith(driverp);
            // pushDeletep(nodep);
            // TODO: Instead of deleting, store in driver?
        }
        checkNoChildren(nodep);
    }
    void visit(AstVar* nodep) {
        if (m_aliasVarps.find(nodep) != m_aliasVarps.end()) {
            // nodep->unlinkFrBack();
            // pushDeletep(nodep);
            // TODO: Instead of deleting, store in driver?
        }
        checkNoChildren(nodep);
    }
    void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    AliasReplacementVisitor(AstNetlist* rootp, const std::unordered_map<Alias, Driver> aliases)
        : m_aliases(aliases) {
        for (const std::pair<Alias, Driver> aliasAndDriver : m_aliases) {
            AstVar* aliasVarp = aliasAndDriver.first->varp();
            UASSERT_OBJ(aliasVarp, aliasAndDriver.first, "AstVarScope for alias has no AstVar");
            m_aliasVarps.insert(aliasVarp);
        }
        iterate(rootp);
    }
    static void replacePublicAliasesWithDrivers(AstNetlist* rootp,
                                                const std::unordered_map<Alias, Driver> aliases) {
        AliasReplacementVisitor{rootp, aliases};
    }
};

void V3Alias::removePublicAliases(AstNetlist* rootp) {
    const std::unordered_map<Alias, Driver> aliases = AliasDetectionVisitor::findAliases(rootp);
    AliasReplacementVisitor::replacePublicAliasesWithDrivers(rootp, aliases);
}