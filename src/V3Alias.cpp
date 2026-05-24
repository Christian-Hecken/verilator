#include "V3Alias.h"

#include "V3Ast.h"
#include "V3Error.h"

#include <unordered_map>
#include <unordered_set>

VL_DEFINE_DEBUG_FUNCTIONS;

using Driver = AstVarScope*;
using Alias = AstVarScope*;
using DriverVarp = AstVar*;
using AliasVarp = AstVar*;

bool isAliasingAssigmnent(const AstNodeAssign* nodep) {
    const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
    const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);
    // Do NOT check for public access - treat all aliases the same to allow for optimization

    return !nodep->isTimingControl() && lhsp && rhsp
           && !lhsp->varp()
                   ->isIO()  // Bandaid fix for t_tri_inz - TODO: Only do this for top-level ports,
                             // or ideally find a way to alias top-level ports
           && !lhsp->isTimingControl() && !rhsp->isTimingControl() && lhsp->access().isWriteOnly()
           && rhsp->access().isReadOnly();
}

void checkNoChildren(const AstNode* nodep) {
    UASSERT_OBJ(!nodep->op1p(), nodep, "Unexpected child under expected leaf node");
    UASSERT_OBJ(!nodep->op2p(), nodep, "Unexpected child under expected leaf node");
    UASSERT_OBJ(!nodep->op3p(), nodep, "Unexpected child under expected leaf node");
    UASSERT_OBJ(!nodep->op4p(), nodep, "Unexpected child under expected leaf node");
}

struct AliasMaps {
    std::unordered_map<Alias, Driver> aliases;
    std::unordered_map<Alias, std::vector<AstNodeAssign*>> aliasingAssignments;
};

class AliasDetectionVisitor : public VNVisitorConst {
    std::unordered_map<Alias, Driver> m_aliases;
    std::unordered_set<Alias> m_multiDrivenVars;
    std::unordered_map<Alias, std::vector<AstNodeAssign*>> m_aliasingAssignments;
    bool m_contextAllowsAliasing{true};

    void visit(AstAlways* nodep) override {
        VL_RESTORER(m_contextAllowsAliasing);
        if (nodep->sentreep()
            || (nodep->keyword() != VAlwaysKwd::ALWAYS_COMB
                && nodep->keyword() != VAlwaysKwd::CONT_ASSIGN))
            m_contextAllowsAliasing = false;
        iterateChildrenConst(nodep);
    }

    void visit(AstCFunc* nodep) override {  // Bandaid fix for t_func_public
        VL_RESTORER(m_contextAllowsAliasing);
        m_contextAllowsAliasing = false;
        iterateChildrenConst(nodep);
    }

    void visit(AstInitial* nodep) override {
        VL_RESTORER(m_contextAllowsAliasing);
        m_contextAllowsAliasing = false;
        iterateChildrenConst(nodep);
    }

    void visit(AstFinal* nodep) override {
        VL_RESTORER(m_contextAllowsAliasing);
        m_contextAllowsAliasing = false;
        iterateChildrenConst(nodep);
    }

    void visit(AstNodeAssign* nodep) override {
        const AstVarRef* lhsp = VN_CAST(nodep->lhsp(), VarRef);
        const AstVarRef* rhsp = VN_CAST(nodep->rhsp(), VarRef);

        if (!lhsp || !rhsp) {
            // TODO: Mark any signals on the LHS as non-eligible

            if (lhsp && !rhsp)  // Signal gets a non-aliasing assignment (e.g. the result of an
                                // addition)
            {
                Alias aliasingCandidate = lhsp->varScopep();
                m_multiDrivenVars.insert(
                    aliasingCandidate);  // TODO: Better naming - means ineligible here
            }

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

        const auto isVirtualInterface = [](AstVarScope* varScopep) {
            AstIfaceRefDType* const ifaceRefp
                = VN_CAST(varScopep->varp()->dtypep()->skipRefp(), IfaceRefDType);
            return ifaceRefp && ifaceRefp->isVirtual();
        };

        if (isVirtualInterface(currentDriver)) {
            UINFO(3, "Signal " << aliasingCandidate->name() << " driven by virtual interface "
                               << currentDriver->name() << ", marking as ineligible");
            m_multiDrivenVars.insert(aliasingCandidate);
            m_aliases.erase(m_aliases.find(aliasingCandidate));
            return;
        }

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
                m_aliasingAssignments[aliasingCandidate].push_back(nodep);
                //pushDeletep(nodep->unlinkFrBack());
                // Remove the now-trivial assignment `x=x` to
                // avoid circular assignment warnings
                // TODO: Do this in the removal visitor, and
                // instead just insert the assignments in a
                // set here, to keep this visitor const
            } else {
                UINFO(3, "Signal " << aliasingCandidate->name()
                                   << " is in context that does not allow aliasing");
                // TODO: Erase from m_aliases
                m_multiDrivenVars.insert(
                    aliasingCandidate);  // TODO: Rename m_multiDrivenVars into
                                         // something like 'm_NotAliasingEligible'
            }
        }
    }
    void visit(AstNode* nodep) override { iterateChildrenConst(nodep); }

    static bool isLoopDriver(const std::unordered_map<Alias, Driver>& aliases, Alias aliasp) {
        Driver driverp = aliases.at(aliasp);
        while (driverp != aliasp) {
            std::unordered_map<Alias, Driver>::const_iterator nextDriverIt = aliases.find(driverp);
            if (nextDriverIt == aliases.end()) return false;
            driverp = nextDriverIt->second;
        }
        return true;
    }

    static AliasMaps preserveLoopDrivers(
        const std::unordered_map<Alias, Driver>& allAliases,
        const std::unordered_map<Alias, std::vector<AstNodeAssign*>>& aliasingAssignments,
        const std::unordered_set<Alias>& ineligibleVars) {
        // TODO: Bandaid fix for t_func_public: Aliases can turn out as ineligible after having
        // been added to the map already, so remove them afterwards
        // -> Could this cause a cascade that needs to be followed?
        std::unordered_map<Alias, Driver> aliasesWithoutIneligibleVars = allAliases;
        std::unordered_map<Alias, std::vector<AstNodeAssign*>>
            aliasingAssignmentsWithoutIneligibleVars = aliasingAssignments;
        for (const std::pair<Alias, Driver> aliasAndDriver : allAliases) {
            const Alias aliasp = aliasAndDriver.first;
            if (ineligibleVars.find(aliasp) != ineligibleVars.end()) {
                UINFO(3, "Removing alias " << aliasp->name() << " driven by "
                                           << aliasesWithoutIneligibleVars.at(aliasp)->name()
                                           << " because it is ineligible");
                aliasesWithoutIneligibleVars.erase(aliasesWithoutIneligibleVars.find(aliasp));
                aliasingAssignmentsWithoutIneligibleVars.erase(
                    aliasingAssignmentsWithoutIneligibleVars.find(aliasp));
            }
        }

        std::unordered_map<Alias, Driver> aliasesWithoutLoopDrivers = aliasesWithoutIneligibleVars;
        std::unordered_map<Alias, std::vector<AstNodeAssign*>>
            aliasingAssignmentsWithoutLoopDrivers = aliasingAssignmentsWithoutIneligibleVars;
        for (const std::pair<Alias, Driver> aliasAndDriver : aliasesWithoutIneligibleVars) {
            const Alias aliasp = aliasAndDriver.first;
            if (isLoopDriver(aliasesWithoutLoopDrivers, aliasp)) {
                UINFO(3, "Preserving alias " << aliasp->name() << " driven by "
                                             << aliasesWithoutLoopDrivers.at(aliasp)->name()
                                             << " because it is part of a loop");
                aliasesWithoutLoopDrivers.erase(aliasesWithoutLoopDrivers.find(aliasp));
                aliasingAssignmentsWithoutLoopDrivers.erase(
                    aliasingAssignmentsWithoutLoopDrivers.find(aliasp));
            }
        }

        return {aliasesWithoutLoopDrivers, aliasingAssignmentsWithoutLoopDrivers};
    }

public:
    AliasDetectionVisitor(AstNetlist* rootp) { iterateConst(rootp); }
    static AliasMaps findAliases(AstNetlist* rootp) {
        AliasDetectionVisitor visitor{rootp};
        const std::unordered_map<Alias, Driver> allAliases = visitor.m_aliases;
        const std::unordered_map<Alias, std::vector<AstNodeAssign*>> aliasingAssignments
            = visitor.m_aliasingAssignments;
        const std::unordered_set<Alias> ineligibleVars = visitor.m_multiDrivenVars;
        return preserveLoopDrivers(allAliases, aliasingAssignments, ineligibleVars);
    }
};

class AliasReplacementVisitor : public VNVisitor {
    const std::unordered_map<Alias, Driver>& m_aliases;
    const std::unordered_map<Alias, std::vector<AstNodeAssign*>>& m_aliasingAssignments;
    std::unordered_map<AliasVarp, DriverVarp> m_aliasVarps;
    void visit(AstVarRef* nodep) override {
        checkNoChildren(nodep);
        UASSERT_OBJ(nodep->varScopep(), nodep, "AstVarRef has no scope");
        Alias aliasingCandidate = nodep->varScopep();
        if (m_aliases.find(aliasingCandidate) != m_aliases.end()) {
            AstVarScope* driverScopep = m_aliases.at(aliasingCandidate);
            AstVarRef* driverp
                = new AstVarRef(driverScopep->fileline(), driverScopep, nodep->access());
            UINFO(3, "Replacing alias " << nodep->name() << " with driver " << driverp->name());
            nodep->replaceWith(driverp);
            pushDeletep(nodep);
        }
    }

    void visit(AstVarScope* nodep) override {
        checkNoChildren(nodep);
        if (m_aliases.find(nodep) != m_aliases.end()) {
            // TODO: Must (somehow) ensure that nothing still points to this scope
            // Idea: Generic traversal over AstNode, if op*p is an AstVarScope*, replace this
            // TODO: Wrong approach - AstVarScopes aren't op*p, but member variables
            // -> Maybe use static reflection?
            nodep->unlinkFrBack();
            pushDeletep(nodep);
        }
    }

    void visit(AstNode* nodep) override { iterateChildren(nodep); }

    void eraseAliasingAssingments() {
        for (const std::pair<Alias const, std::vector<AstNodeAssign*>>& aliasAndAssignments :
             m_aliasingAssignments) {
            const std::vector<AstNodeAssign*>& assignments = aliasAndAssignments.second;
            for (AstNodeAssign* const assignment : assignments) {
                UINFO(3, "Removing aliasing assignment " << assignment->name());
                pushDeletep(assignment->unlinkFrBack());
            }
        }
    }

    void propagatePublicToDrivers(AliasVarp aliasVarp) {
        std::unordered_set<DriverVarp> visited;
        visited.insert(aliasVarp);

        // Find the final driver in the chain
        DriverVarp driverVarp = m_aliasVarps.at(aliasVarp);
        while (m_aliasVarps.find(driverVarp) != m_aliasVarps.end()) {
            if (visited.find(driverVarp) != visited.end()) {
                return;  // Cycle detected, stop
            }
            visited.insert(driverVarp);
            driverVarp = m_aliasVarps.at(driverVarp);
        }

        // Propagate public flags from the alias to the final driver
        if (aliasVarp->isSigPublic()) driverVarp->sigPublic(true);
        if (aliasVarp->isSigModPublic()) driverVarp->sigModPublic(true);
        if (aliasVarp->isSigUserRdPublic()) driverVarp->sigUserRdPublic(true);
        if (aliasVarp->isSigUserRWPublic()) driverVarp->sigUserRWPublic(true);
        if (aliasVarp->isContinuously()) driverVarp->isContinuously(true);
    }

    void ensureDriversPublic() {
        for (const std::pair<AliasVarp, DriverVarp> aliasAndDriver : m_aliasVarps) {
            const AliasVarp aliasp = aliasAndDriver.first;
            propagatePublicToDrivers(aliasp);
        }
    }

public:
    AliasReplacementVisitor(AstNetlist* rootp, const AliasMaps& aliasMaps)
        : m_aliases(aliasMaps.aliases)
        , m_aliasingAssignments(aliasMaps.aliasingAssignments) {
        for (const std::pair<Alias, Driver> aliasAndDriver : m_aliases) {
            AstVar* aliasVarp = aliasAndDriver.first->varp();
            AstVar* driverVarp = aliasAndDriver.second->varp();
            UASSERT_OBJ(aliasVarp, aliasAndDriver.first, "AstVarScope for alias has no AstVar");
            m_aliasVarps[aliasVarp] = driverVarp;
        }
        ensureDriversPublic();
        eraseAliasingAssingments();
        iterate(rootp);
    }
    static void replacePublicAliasesWithDrivers(AstNetlist* rootp, const AliasMaps& aliasMaps) {
        AliasReplacementVisitor{rootp, aliasMaps};
    }
};

void V3Alias::removePublicAliases(AstNetlist* rootp) {
    const AliasMaps aliasMaps = AliasDetectionVisitor::findAliases(rootp);

    for (const std::pair<Alias, Driver> aliasAndDriver : aliasMaps.aliases) {
        const Alias aliasp = aliasAndDriver.first;
        const Alias driverp = aliasAndDriver.second;
        UINFO(3, "Found alias " << aliasp->name() << " driven by " << driverp->name());
    }

    AliasReplacementVisitor::replacePublicAliasesWithDrivers(rootp, aliasMaps);

    V3Global::dumpCheckGlobalTree("alias", 0, dumpTreeEitherLevel() >= 3);
}
