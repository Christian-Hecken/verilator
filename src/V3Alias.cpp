#include "V3Alias.h"

#include "V3Ast.h"

VL_DEFINE_DEBUG_FUNCTIONS;

using Driver = AstVarScope*;
using Alias = AstVarScope*;

bool isAliasingAssigmnent(const AstNodeAssign* nodep) {
    const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
    const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);
    // Do NOT check for public access - treat all aliases the same to allow for optimization

    return !nodep->isTimingControl() && lhsp && rhsp && !lhsp->isTimingControl()
           && !rhsp->isTimingControl() && lhsp->access().isWriteOnly()
           && rhsp->access().isReadOnly();
}

void checkNoChildren(const AstNode* nodep) {
    UASSERT_OBJ(!nodep->op1p(), nodep, "Unexpected child under expected leaf node");
    UASSERT_OBJ(!nodep->op2p(), nodep, "Unexpected child under expected leaf node");
    UASSERT_OBJ(!nodep->op3p(), nodep, "Unexpected child under expected leaf node");
    UASSERT_OBJ(!nodep->op4p(), nodep, "Unexpected child under expected leaf node");
}

class AliasDetectionVisitor : public VNVisitorConst {
    std::unordered_map<Alias, Driver> m_aliases;
    std::unordered_set<Alias> m_multiDrivenVars;
    bool m_contextAllowsAliasing{true};

    void visit(AstAlways* nodep) {
        VL_RESTORER(m_contextAllowsAliasing);
        if (nodep->sentreep()
            || (nodep->keyword() != VAlwaysKwd::ALWAYS_COMB
                && nodep->keyword() != VAlwaysKwd::CONT_ASSIGN))
            m_contextAllowsAliasing = false;
        iterateChildrenConst(nodep);
    }

    void visit(AstInitial* nodep) {
        VL_RESTORER(m_contextAllowsAliasing);
        m_contextAllowsAliasing = false;
        iterateChildrenConst(nodep);
    }

    void visit(AstFinal* nodep) {
        VL_RESTORER(m_contextAllowsAliasing);
        m_contextAllowsAliasing = false;
        iterateChildrenConst(nodep);
    }

    void visit(AstNodeAssign* nodep) override {
        const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
        const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);

        if (!lhsp || !rhsp) {
            // TODO: Mark any signals on the LHS as non-eligible

            // WIP: Special case workaround for AstNodeSel*
            const AstNodeSel* lhsNodep = VN_CAST(nodep->lhsp(), NodeSel);
            if (!lhsNodep) return;
            const AstVarRef* lhsRefp = VN_CAST(lhsNodep->fromp(), VarRef);
            if (!lhsRefp) return;
            Alias aliasingCandidate = lhsRefp->varScopep();
            m_multiDrivenVars.insert(
                aliasingCandidate);  // TODO: Better naming - means ineligible here
            if (m_aliases.find(aliasingCandidate) != m_aliases.end())
                m_aliases.erase(m_aliases.find(aliasingCandidate));
            return;
        }

        UASSERT_OBJ(lhsp->varScopep(), lhsp,
                    "Assignment lhs var reference has no associated AstVarScope");
        UASSERT_OBJ(rhsp->varScopep(), rhsp,
                    "Assignment rhs var reference has no associated AstVarScope");
        Alias aliasingCandidate = lhsp->varScopep();
        Driver currentDriver = rhsp->varScopep();

        // TODO: If non-aliasing assignment (or in context that doesn't allow aliasing),
        // immediately put it into the non-eligible category Else it could happen that one aliasing
        // assignment is picked even though there are several non-aliasing assignments
        if (isAliasingAssigmnent(nodep)
            && m_multiDrivenVars.find(aliasingCandidate) == m_multiDrivenVars.end()) {
            Driver previousDriver = [this, aliasingCandidate]() {
                auto it = m_aliases.find(aliasingCandidate);
                return (it != m_aliases.end()) ? it->second : nullptr;
            }();
            UINFO(3, "Found signal " << aliasingCandidate->name() << ", previous driver "
                                     << (previousDriver ? previousDriver->name() : "<NONE>")
                                     << ", current driver " << currentDriver->name());
            if (previousDriver && currentDriver != previousDriver) {
                UINFO(3, "Signal " << aliasingCandidate->name()
                                   << " has different driver than before, marking as ineligible");
                m_multiDrivenVars.insert(aliasingCandidate);
                m_aliases.erase(m_aliases.find(aliasingCandidate));
            } else if (m_contextAllowsAliasing) {
                UINFO(3, "Signal " << aliasingCandidate->name()
                                   << " can be aliased, marking as alias of driver "
                                   << currentDriver->name());
                m_aliases[aliasingCandidate] = currentDriver;
            } else {
                UINFO(3, "Signal " << aliasingCandidate->name()
                                   << " is in context that does not allow aliasing");
                // TODO: Erase from m_aliases
                m_multiDrivenVars.insert(
                    aliasingCandidate);  // TODO: Rename m_multiDrivenVars into something like
                                         // 'm_NotAliasingEligible'
            }
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
        checkNoChildren(nodep);
        UASSERT_OBJ(nodep->varScopep(), nodep, "AstVarRef has no scope");
        Alias aliasingCandidate = nodep->varScopep();
        if (m_aliases.find(aliasingCandidate) != m_aliases.end()) {
            AstVarScope* driverScopep = m_aliases.at(aliasingCandidate);
            AstVarRef* driverp
                = new AstVarRef(driverScopep->fileline(), driverScopep, nodep->access());
            nodep->replaceWith(driverp);
            pushDeletep(nodep);
        }
    }
    void visit(AstVarScope* nodep) {
        checkNoChildren(nodep);
        if (m_aliases.find(nodep) != m_aliases.end()) {
            // TODO: Must (somehow) ensure that nothing still points to this scope
            // Idea: Generic traversal over AstNode, if op*p is an AstVarScope*, replace this
            // TODO: Wrong approach - AstVarScopes aren't op*p, but member variables
            // -> Maybe use static reflection?
            Driver driverp = m_aliases.find(nodep)->second;
            nodep->unlinkFrBack();
            // driverp->addAlias(nodep); // TODO: Add to varp instead
            pushDeletep(nodep);
        }
    }
    void visit(AstVar* nodep) {
        checkNoChildren(nodep);
        if (m_aliasVarps.find(nodep) != m_aliasVarps.end()) {
            nodep->unlinkFrBack();
            pushDeletep(nodep);
            // TODO: Instead of deleting, store in driver?
            // -> Set/vector of AstNode* -> Can store both AstVar* and AstVarScope* in it
        }
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

    for (const std::pair<Alias, Driver> aliasAndDriver : aliases) {
        const Alias aliasp = aliasAndDriver.first;
        const Alias driverp = aliasAndDriver.second;
        UINFO(3, "Found alias " << aliasp->name() << " driven by " << driverp->name());
    }

    AliasReplacementVisitor::replacePublicAliasesWithDrivers(rootp, aliases);

    V3Global::dumpCheckGlobalTree("alias", 0, dumpTreeEitherLevel() >= 3);
}