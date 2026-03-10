// -*- mode: C++; c-file-style: "cc-mode" -*-
//*************************************************************************
// DESCRIPTION: Verilator: Covert forceable signals, process force/release
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
//  V3Force's Transformations:
//
//  For each forceable net with name "<name>":
//      add 2 extra signals:
//          - <name>__VforceEn: a var with same type as signal, which is the bitwise force enable
//          - <name>__VforceVal: a var with same type as signal, which is the forced value
//      add an initial statement:
//          initial <name>__VforceEn = 0;
//      replace all READ references to <name> with the inline mux expression:
//          (<name>__VforceEn ? <name>__VforceVal : <name>)
//
//  Replace each AstAssignForce with 2 assignments:
//      - <lhs>__VforceEn = 1
//      - <lhs>__VforceVal = <rhs>
//
//  Replace each AstRelease with 1 or 2 assignments:
//      - <lhs>__VforceEn = 0                  (for variables: also <lhs> = <lhs>__VforceVal)
//
//  After each WRITE of forced RHS
//      reevaluate <lhs>__VforceVal to support VarRef rollback after release
//*************************************************************************

#include "V3PchAstNoMT.h"  // VL_MT_DISABLED_CODE_UNIT

#include "V3Force.h"

#include "V3AstUserAllocator.h"
#include "V3UniqueNames.h"

VL_DEFINE_DEBUG_FUNCTIONS;

//######################################################################
// Convert force/release statements and signals marked 'forceable'

class ForceState final {
    constexpr static int ELEMENTS_MAX = 1000;
    // TYPES
    struct ForceComponentsVar final {
        AstVar* const m_valVarp;  // Forced value
        AstVar* const m_enVarp;  // Force enabled signal
        explicit ForceComponentsVar(AstVar* varp)
            : m_valVarp{new AstVar{varp->fileline(), VVarType::VAR, varp->name() + "__VforceVal",
                                   varp->dtypep()}}
            , m_enVarp{new AstVar{varp->fileline(), VVarType::VAR, varp->name() + "__VforceEn",
                                  getEnVarpDTypep(varp)}} {
            m_enVarp->addNext(m_valVarp);
            varp->addNextHere(m_enVarp);
        }
    };

public:
    struct ForceComponentsVarScope final {
        AstVarScope* const m_valVscp;  // Forced value
        AstVarScope* const m_enVscp;  // Force enabled signal
        V3UniqueNames m_iterNames;  // Names for loop iteration variables (unpacked arrays)
        explicit ForceComponentsVarScope(AstVarScope* vscp, ForceComponentsVar& fcv)
            : m_valVscp{new AstVarScope{vscp->fileline(), vscp->scopep(), fcv.m_valVarp}}
            , m_enVscp{new AstVarScope{vscp->fileline(), vscp->scopep(), fcv.m_enVarp}}
            , m_iterNames{"__VForceIter"} {
            m_enVscp->addNext(m_valVscp);
            vscp->addNextHere(m_enVscp);

            FileLine* const flp = vscp->fileline();

            // Add initialization of the enable signal to zero
            AstActive* const activeInitp = new AstActive{
                flp, "force-init", new AstSenTree{flp, new AstSenItem{flp, AstSenItem::Static{}}}};
            activeInitp->senTreeStorep(activeInitp->sentreep());
            AstVarRef* const enRefp = new AstVarRef{flp, m_enVscp, VAccess::WRITE};
            AstNodeStmt* const enInitStmtsp = genEnZeroInitStmtsRecursep(enRefp);
            activeInitp->addStmtsp(new AstInitial{flp, enInitStmtsp});
            vscp->scopep()->addBlocksp(activeInitp);
        }
        // Generate statements that zero-initialise a given LHS expression recursively,
        // handling packed/integral types, structs, and unpacked arrays.
        AstNodeStmt* genEnZeroInitStmtsRecursep(AstNodeExpr* const lhsp) {
            FileLine* const flp = lhsp->fileline();
            const AstNodeDType* const lhsDtypep = lhsp->dtypep()->skipRefp();
            if (lhsDtypep->isIntegralOrPacked() || VN_IS(lhsDtypep, BasicDType)) {
                V3Number zero{m_enVscp, lhsp->dtypep()->width()};
                return new AstAssign{flp, lhsp, new AstConst{flp, zero}};
            } else if (const AstStructDType* const structDtypep
                       = VN_CAST(lhsDtypep, StructDType)) {
                AstNodeStmt* stmtsp = nullptr;
                bool firstIter = true;
                for (AstMemberDType* mdtp = structDtypep->membersp(); mdtp;
                     mdtp = VN_AS(mdtp->nextp(), MemberDType)) {
                    AstNodeExpr* const lhsCopyp = firstIter ? lhsp : lhsp->cloneTreePure(false);
                    AstStructSel* const structSelp = new AstStructSel{flp, lhsCopyp, mdtp->name()};
                    structSelp->dtypep(mdtp);
                    AstNodeStmt* const stmt = genEnZeroInitStmtsRecursep(structSelp);
                    stmtsp = firstIter ? stmt : stmtsp->addNext(stmt);
                    firstIter = false;
                }
                return stmtsp;
            } else if (const AstUnpackArrayDType* const arrayDtypep
                       = VN_CAST(lhsDtypep, UnpackArrayDType)) {
                AstVar* const loopVarp
                    = new AstVar{flp, VVarType::MODULETEMP,
                                 m_iterNames.get(m_enVscp->varp()->name()), VFlagBitPacked{}, 32};
                m_enVscp->varp()->addNext(loopVarp);
                AstVarScope* const loopVarScopep
                    = new AstVarScope{flp, m_enVscp->scopep(), loopVarp};
                m_enVscp->addNext(loopVarScopep);
                AstVarRef* const readRefp = new AstVarRef{flp, loopVarScopep, VAccess::READ};
                AstNodeStmt* const currInitp = new AstAssign{
                    flp, new AstVarRef{flp, loopVarScopep, VAccess::WRITE}, new AstConst{flp, 0}};
                AstLoop* const currWhilep = new AstLoop{flp};
                currInitp->addNextHere(currWhilep);
                AstLoopTest* const loopTestp = new AstLoopTest{
                    flp, currWhilep,
                    new AstNeq{
                        flp, readRefp,
                        new AstConst{flp, static_cast<uint32_t>(arrayDtypep->elementsConst())}}};
                currWhilep->addStmtsp(loopTestp);
                AstArraySel* const lhsSelp
                    = new AstArraySel{flp, lhsp, readRefp->cloneTree(false)};
                AstNodeStmt* const loopBodyp = genEnZeroInitStmtsRecursep(lhsSelp);
                currWhilep->addStmtsp(loopBodyp);
                AstAssign* const currIncrp = new AstAssign{
                    flp, new AstVarRef{flp, loopVarScopep, VAccess::WRITE},
                    new AstAdd{flp, readRefp->cloneTree(false), new AstConst{flp, 1}}};
                currWhilep->addStmtsp(currIncrp);
                return currInitp;
            } else {
                lhsDtypep->v3fatalSrc("Unhandled type");
            }
        }
        static AstNodeExpr* wrapIntoExprp(AstVarRef* const refp, AstNodeExpr* const exprp,
                                          AstVarRef* const varRefToReplacep) {
            // Return a copy of exprp in which varRefToReplacep is replaced with refp
            if (exprp == varRefToReplacep) {
                return refp;
            } else {
                AstNodeExpr* const copiedExprp = exprp->cloneTreePure(false);
                AstNode* const oldRefp = varRefToReplacep->clonep();
                varRefToReplacep->clonep()->replaceWith(refp);
                oldRefp->deleteTree();
                return copiedExprp;
            }
        }
        AstNodeExpr* forcedUpdate(AstVarScope* const vscp, AstNodeExpr* exprp = nullptr,
                                  AstVarRef* const varRefToReplacep = nullptr) const {
            FileLine* const flp = vscp->fileline();
            AstVarRef* origRefp = new AstVarRef{flp, vscp, VAccess::READ};
            ForceState::markNonReplaceable(origRefp);
            AstNodeExpr* const origExprp = wrapIntoExprp(origRefp, exprp, varRefToReplacep);
            AstNodeExpr* const enExprp = wrapIntoExprp(new AstVarRef{flp, m_enVscp, VAccess::READ},
                                                       exprp, varRefToReplacep);
            AstNodeExpr* const valExprp = wrapIntoExprp(
                new AstVarRef{flp, m_valVscp, VAccess::READ}, exprp, varRefToReplacep);
            if (ForceState::isRangedDType(vscp)) {
                return new AstOr{
                    flp, new AstAnd{flp, enExprp, valExprp},
                    new AstAnd{flp, new AstNot{flp, enExprp->cloneTreePure(false)}, origExprp}};
            }
            return new AstCond{flp, enExprp, valExprp, origExprp};
        }
    };

private:
    // NODE STATE
    //  AstNodeDType::user1p  -> AstNodeDType*, dtype created for __En variables
    //  AstVar::user1p        -> ForceComponentsVar* instance (via m_forceComponentsVar)
    //  AstVarScope::user1p   -> ForceComponentsVarScope* instance (via m_forceComponentsVarScope)
    //  AstVarRef::user2      -> Flag indicating not to replace reference
    //  AstAssign::user2      -> Flag indicating that assignment was created for AstRelease
    //  AstVarScope::user3p   -> AstAssign*, the assignment <lhs>__VforceVal = <rhs>
    const VNUser1InUse m_user1InUse;
    const VNUser2InUse m_user2InUse;
    const VNUser3InUse m_user3InUse;
    AstUser1Allocator<AstVar, ForceComponentsVar> m_forceComponentsVar;
    AstUser1Allocator<AstVarScope, ForceComponentsVarScope> m_forceComponentsVarScope;
    std::unordered_map<const AstVarScope*,
                       std::pair<std::unordered_set<AstVarScope*>, std::vector<AstVarScope*>>>
        m_valVscps;
    // `valVscp` force components of a forced RHS

    static size_t checkIfDTypeSupportedRecurse(const AstNodeDType* const dtypep,
                                               const AstVar* const varp) {
        // Checks if force stmt is supported on all subtypes
        // and returns number of unpacked elements
        const AstNodeDType* const dtp = dtypep->skipRefp();
        if (const AstUnpackArrayDType* const udtp = VN_CAST(dtp, UnpackArrayDType)) {
            const size_t elemsInSubDType = checkIfDTypeSupportedRecurse(udtp->subDTypep(), varp);
            return udtp->elementsConst() * elemsInSubDType;
        } else if (const AstStructDType* const sdtp = VN_CAST(dtp, StructDType)) {
            size_t elemCount = 0;
            for (const AstMemberDType* mdtp = sdtp->membersp(); mdtp;
                 mdtp = VN_AS(mdtp->nextp(), MemberDType)) {
                elemCount += checkIfDTypeSupportedRecurse(mdtp->subDTypep(), varp);
            }
            return elemCount;
        } else if (const AstBasicDType* const bdtp = VN_CAST(dtp, BasicDType)) {
            if (bdtp->isString() || bdtp->isEvent() || bdtp->keyword() == VBasicDTypeKwd::CHANDLE
                || bdtp->keyword() == VBasicDTypeKwd::TIME) {
                varp->v3warn(E_UNSUPPORTED, "Forcing variable of unsupported type: "
                                                << varp->dtypep()->prettyTypeName());
            }
            return 1;
        } else if (!dtp->isIntegralOrPacked()) {
            varp->v3warn(E_UNSUPPORTED, "Forcing variable of unsupported type: "
                                            << varp->dtypep()->prettyTypeName());
            return 1;
        } else {
            // All packed types are supported
            return 1;
        }
    }
    static AstNodeDType* getEnVarpDTypep(AstVar* const varp) {
        AstNodeDType* const origDTypep = varp->dtypep()->skipRefp();
        if (origDTypep->user1p()) return VN_AS(origDTypep->user1p(), NodeDType);
        const size_t unpackElemNum = checkIfDTypeSupportedRecurse(origDTypep, varp);
        if (unpackElemNum > ELEMENTS_MAX) {
            varp->v3warn(E_UNSUPPORTED, "Unsupported: Force of variable with "
                                        ">= "
                                            << ELEMENTS_MAX << " unpacked elements");
            return origDTypep;
        }
        return getEnVarpDTypeRecursep(varp, origDTypep);
    }
    static AstNodeDType* getEnVarpDTypeRecursep(AstVar* const varp, AstNodeDType* const dtypep) {
        if (dtypep->user1p()) return VN_AS(dtypep->user1p(), NodeDType);
        if (AstNodeArrayDType* const arrp = VN_CAST(dtypep, NodeArrayDType)) {
            AstNodeDType* const subDTypep = arrp->subDTypep()->skipRefp();
            AstNodeDType* const enSubDTypep = getEnVarpDTypeRecursep(varp, subDTypep);
            if (subDTypep != enSubDTypep) {
                AstNodeArrayDType* enArrp;
                if (VN_IS(arrp, UnpackArrayDType)) {
                    enArrp = new AstUnpackArrayDType{arrp->fileline(), enSubDTypep,
                                                     arrp->rangep()->cloneTree(false)};
                } else if (VN_IS(arrp, PackArrayDType)) {
                    enArrp = new AstPackArrayDType{arrp->fileline(), enSubDTypep,
                                                   arrp->rangep()->cloneTree(false)};
                } else {
                    varp->v3fatalSrc("Unsupported: Force of variable of unhandled data type");
                    return dtypep;
                }
                dtypep->user1p(enArrp);
                v3Global.rootp()->typeTablep()->addTypesp(enArrp);
                return enArrp;
            } else {
                dtypep->user1p(dtypep);
                return dtypep;
            }
        } else if (AstBasicDType* const basicp = VN_CAST(dtypep, BasicDType)) {
            if (basicp->isBit()) {
                dtypep->user1p(dtypep);
                return dtypep;
            } else {
                AstNodeDType* const bitDtp = varp->findBitRangeDType(
                    basicp->declRange(), basicp->elements(), VSigning::UNSIGNED);
                dtypep->user1p(bitDtp);
                return bitDtp;
            }
        } else if (AstNodeUOrStructDType* const structp = VN_CAST(dtypep, NodeUOrStructDType)) {
            std::vector<AstMemberDType*> enMemberDTypes;
            bool changed = false;
            for (AstMemberDType* mdtp = structp->membersp(); mdtp;
                 mdtp = VN_AS(mdtp->nextp(), MemberDType)) {
                AstNodeDType* const subMdtp = mdtp->subDTypep()->skipRefp();
                AstNodeDType* const enSubMdtp = getEnVarpDTypeRecursep(varp, subMdtp);
                if (subMdtp != enSubMdtp) {
                    changed = true;
                    AstMemberDType* const enMdtp
                        = new AstMemberDType{mdtp->fileline(), mdtp->name(), enSubMdtp};
                    enMdtp->dtypep(enSubMdtp);
                    enMemberDTypes.push_back(enMdtp);
                } else {
                    enMemberDTypes.push_back(mdtp->cloneTreePure(false));
                }
            }
            if (changed) {
                const bool packed = structp->packed();
                AstNodeUOrStructDType* enStructp;
                if (VN_IS(structp, StructDType)) {
                    enStructp = new AstStructDType{structp->fileline(),
                                                   packed ? VSigning::SIGNED : VSigning::NOSIGN};
                } else if (VN_IS(structp, UnionDType) && packed) {
                    const AstUnionDType* const unionp = VN_AS(structp, UnionDType);
                    enStructp = new AstUnionDType{unionp->fileline(), unionp->isSoft(),
                                                  unionp->isTagged(), VSigning::SIGNED};
                } else {
                    varp->v3fatalSrc("Unsupported: Force of variable of unhandled data type");
                    return dtypep;
                }
                int width = 0;
                if (packed) {
                    for (const auto& memberp : enMemberDTypes) {
                        enStructp->addMembersp(memberp);
                        const int memberWidth = memberp->width();
                        if (VN_IS(structp, StructDType)) {
                            width += memberWidth;
                        } else {
                            width = std::max(width, memberWidth);
                        }
                    }
                } else {
                    for (const auto& memberp : enMemberDTypes) enStructp->addMembersp(memberp);
                    width = 1;
                }
                v3Global.rootp()->typeTablep()->addTypesp(enStructp);
                enStructp->name(structp->name() + "__VforceEn_t");
                enStructp->dtypep(enStructp);
                enStructp->widthForce(width, width);
                enStructp->classOrPackagep(structp->classOrPackagep());
                dtypep->user1p(enStructp);
                AstTypedef* const typedefp
                    = new AstTypedef{enStructp->fileline(), enStructp->name(), enStructp,
                                     VN_IS(enStructp->classOrPackagep(), Class)};
                varp->addNextHere(typedefp);
                return enStructp;
            } else {
                for (const auto& memberp : enMemberDTypes) memberp->deleteTree();
                dtypep->user1p(dtypep);
                return dtypep;
            }
        }
        varp->v3fatalSrc("Unsupported: Force of variable of unhandled data type");
        return dtypep;
    }

public:
    // CONSTRUCTORS
    ForceState() = default;
    VL_UNCOPYABLE(ForceState);

    // STATIC METHODS
    static bool isRangedDType(const AstNode* const nodep) {
        // If ranged we need a multibit enable to support bit-by-bit part-select forces,
        // otherwise forcing a real or other opaque dtype and need a single bit enable.
        const AstBasicDType* const basicp = nodep->dtypep()->skipRefp()->basicp();
        return basicp && basicp->isRanged();
    }
    static bool isNotReplaceable(const AstVarRef* const nodep) { return nodep->user2(); }
    static void markNonReplaceable(AstVarRef* const nodep) { nodep->user2SetOnce(); }

    // Get all ValVscps for a VarScope
    const std::vector<AstVarScope*>* getValVscps(AstVarRef* const refp) const {
        auto it = m_valVscps.find(refp->varScopep());
        if (it != m_valVscps.end()) return &(it->second.second);
        return nullptr;
    }

    // Add a ValVscp for a VarScope
    void addValVscp(AstVarRef* const refp, AstVarScope* const valVscp) {
        if (m_valVscps[refp->varScopep()].first.find(valVscp)
            != m_valVscps[refp->varScopep()].first.end())
            return;
        m_valVscps[refp->varScopep()].first.emplace(valVscp);
        m_valVscps[refp->varScopep()].second.push_back(valVscp);
    }

    // METHODS
    const ForceComponentsVarScope& getForceComponents(AstVarScope* vscp) {
        AstVar* const varp = vscp->varp();
        return m_forceComponentsVarScope(vscp, vscp, m_forceComponentsVar(varp, varp));
    }
    ForceComponentsVarScope* tryGetForceComponents(AstVarRef* nodep) const {
        return m_forceComponentsVarScope.tryGet(nodep->varScopep());
    }
    void setValVscpAssign(AstVarScope* valVscp, AstAssign* rhsExpr) { valVscp->user3p(rhsExpr); }
    AstAssign* getValVscpAssign(AstVarScope* valVscp) const {
        return VN_CAST(valVscp->user3p(), Assign);
    }
};

class ForceConvertVisitor final : public VNVisitor {
    // STATE
    ForceState& m_state;

    // STATIC METHODS
    // Replace each AstNodeVarRef in the given 'nodep' that writes a variable by transforming the
    // referenced AstVarScope with the given function.
    static void transformWritenVarScopes(AstNode* nodep,
                                         std::function<AstVarScope*(AstVarScope*)> f) {
        UASSERT_OBJ(nodep->backp(), nodep, "Must have backp, otherwise will be lost if replaced");
        nodep->foreach([&f](AstNodeVarRef* refp) {
            if (refp->access() != VAccess::WRITE) return;
            // TODO: this is not strictly speaking safe for some complicated lvalues, eg.:
            //       'force foo[a(cnt)] = 1;', where 'cnt' is an out parameter, but it will
            //       do for now...
            refp->replaceWith(
                new AstVarRef{refp->fileline(), f(refp->varScopep()), VAccess::WRITE});
            VL_DO_DANGLING(refp->deleteTree(), refp);
        });
    }

    // VISITORS
    void visit(AstNode* nodep) override { iterateChildren(nodep); }

    void visit(AstAssignForce* nodep) override {
        // The AstAssignForce node will be removed for sure
        FileLine* const flp = nodep->fileline();
        AstNodeExpr* const lhsp = nodep->lhsp();  // The LValue we are forcing
        AstNodeExpr* const rhsp = nodep->rhsp();  // The value we are forcing it to
        VNRelinker relinker;
        nodep->unlinkFrBack(&relinker);
        VL_DO_DANGLING(pushDeletep(nodep), nodep);

        // Set corresponding enable signals to ones
        V3Number ones{lhsp, ForceState::isRangedDType(lhsp) ? lhsp->width() : 1};
        ones.setAllBits1();
        AstAssign* const setEnp
            = new AstAssign{flp, lhsp->cloneTreePure(false), new AstConst{rhsp->fileline(), ones}};
        transformWritenVarScopes(setEnp->lhsp(), [this](AstVarScope* vscp) {
            return m_state.getForceComponents(vscp).m_enVscp;
        });
        // Set corresponding value signals to the forced value
        AstAssign* const setValp
            = new AstAssign{flp, lhsp->cloneTreePure(false), rhsp->cloneTreePure(false)};
        transformWritenVarScopes(setValp->lhsp(), [this, rhsp, setValp](AstVarScope* vscp) {
            AstVarScope* const valVscp = m_state.getForceComponents(vscp).m_valVscp;
            m_state.setValVscpAssign(valVscp, setValp);
            rhsp->foreach([valVscp, this](AstVarRef* refp) { m_state.addValVscp(refp, valVscp); });
            return valVscp;
        });

        setEnp->addNext(setValp);
        // Unlink lhsp/rhsp that are no longer needed by setRdp (removed)
        lhsp->unlinkFrBack()->deleteTree();
        rhsp->unlinkFrBack()->deleteTree();
        relinker.relink(setEnp);
    }

    void visit(AstRelease* nodep) override {
        FileLine* const flp = nodep->fileline();
        AstNodeExpr* const lhsp = nodep->lhsp();  // The LValue we are releasing
        // The AstRelease node will be removed for sure
        VNRelinker relinker;
        nodep->unlinkFrBack(&relinker);
        VL_DO_DANGLING(pushDeletep(nodep), nodep);

        // Set corresponding enable signals to zero
        V3Number zero{lhsp, ForceState::isRangedDType(lhsp) ? lhsp->width() : 1};
        zero.setAllBits0();
        AstAssign* const resetEnp
            = new AstAssign{flp, lhsp->cloneTreePure(false), new AstConst{lhsp->fileline(), zero}};
        transformWritenVarScopes(resetEnp->lhsp(), [this](AstVarScope* vscp) {
            return m_state.getForceComponents(vscp).m_enVscp;
        });

        // IEEE 1800-2023 10.6.2: When released, then if the variable is not driven by a continuous
        // assignment and does not currently have an active procedural continuous assignment, the
        // variable shall not immediately change value and shall maintain its current value until
        // the next procedural assignment to the variable is executed. Releasing a variable that is
        // driven by a continuous assignment or currently has an active assign procedural
        // continuous assignment shall reestablish that assignment and schedule a reevaluation in
        // the continuous assignment's scheduling region.
        AstVarRef* const refp
            = VN_AS(AstNodeVarRef::varRefLValueRecurse(lhsp->unlinkFrBack()), VarRef);
        AstVarScope* const vscp = refp->varScopep();

        if (vscp->varp()->isContinuously()) {
            // Net: just clear the enable; the inline mux at each read site automatically
            // reverts to the net's driven value once __VforceEn is zero.
            relinker.relink(resetEnp);
        } else {
            // Variable: IEEE 1800-2023 10.6.2 - variable retains its value at release time.
            // Write the currently-forced value back into the (element of the) variable before
            // clearing the enable.  lhsp is the detached element-access or bare VarRef; refp
            // is the innermost VarRef within it.
            const ForceState::ForceComponentsVarScope& fcp = m_state.getForceComponents(vscp);
            // For element-level release (e.g., release arr[i][j]), build an element-level mux;
            // for whole-variable release, build a whole-variable mux.
            AstNodeExpr* const rhsp
                = (lhsp == refp) ? fcp.forcedUpdate(vscp) : fcp.forcedUpdate(vscp, lhsp, refp);
            AstAssign* const resetRdp = new AstAssign{flp, lhsp, rhsp};
            resetRdp->user2(true);
            resetRdp->addNext(resetEnp);
            relinker.relink(resetRdp);
        }
    }

    void visit(AstVarScope* nodep) override {
        // If this signal is marked externally forceable, create the public force signals
        if (nodep->varp()->isForceable()) {
            if (VN_IS(nodep->varp()->dtypeSkipRefp(), UnpackArrayDType)) {
                nodep->varp()->v3warn(
                    E_UNSUPPORTED,
                    "Unsupported: Forcing unpacked arrays: " << nodep->varp()->name());  // (#4735)
                return;
            }

            const AstBasicDType* const bdtypep = nodep->varp()->basicp();
            const bool strtype = bdtypep && bdtypep->keyword() == VBasicDTypeKwd::STRING;
            if (strtype) {
                nodep->varp()->v3error(
                    "Forcing strings is not permitted: " << nodep->varp()->name());
            }

            const ForceState::ForceComponentsVarScope& fc = m_state.getForceComponents(nodep);
            fc.m_enVscp->varp()->sigUserRWPublic(true);
            fc.m_valVscp->varp()->sigUserRWPublic(true);
        }
    }

public:
    // CONSTRUCTOR
    // cppcheck-suppress constParameterCallback
    ForceConvertVisitor(AstNetlist* nodep, ForceState& state)
        : m_state{state} {
        // Transform all force and release statements
        iterateAndNextNull(nodep->modulesp());
    }
};

class ForceReplaceVisitor final : public VNVisitor {
    // STATE
    const ForceState& m_state;
    AstNodeStmt* m_stmtp = nullptr;
    bool m_inLogic = false;
    bool m_releaseRhs = false;  // Inside RHS of assignment created for release statement

    // METHODS
    void iterateLogic(AstNode* logicp) {
        VL_RESTORER(m_inLogic);
        m_inLogic = true;
        iterateChildren(logicp);
    }

    // VISITORS
    void visit(AstNodeStmt* nodep) override {
        VL_RESTORER(m_stmtp);
        m_stmtp = nodep;
        iterateChildren(nodep);
    }
    void visit(AstAssign* nodep) override {
        VL_RESTORER(m_stmtp);
        VL_RESTORER(m_releaseRhs);
        m_stmtp = nodep;
        iterate(nodep->lhsp());
        m_releaseRhs = nodep->user2();
        iterate(nodep->rhsp());
    }
    void visit(AstCFunc* nodep) override { iterateLogic(nodep); }
    void visit(AstCoverToggle* nodep) override { iterateLogic(nodep); }
    void visit(AstNodeProcedure* nodep) override { iterateLogic(nodep); }
    void visit(AstAlways* nodep) override {
        // TODO: this is the old behavioud prior to moving AssignW under Always.
        // Review if this is appropriate or if we are missing something...
        if (nodep->keyword() == VAlwaysKwd::CONT_ASSIGN) {
            iterateChildren(nodep);
            return;
        }
        iterateLogic(nodep);
    }
    void visit(AstSenItem* nodep) override { iterateLogic(nodep); }
    void visit(AstVarRef* nodep) override {
        if (ForceState::isNotReplaceable(nodep)) return;

        switch (nodep->access()) {
        case VAccess::READ: {
            // Replace read reference to a forceable signal with the inline mux expression:
            //   (__VforceEn ? __VforceVal : <name>)
            // This avoids a stale cached value and makes reads always current.
            // For non-packed (unpacked array / struct) signals the mux must be distributed
            // over the full element-access path so that each branch indexes the same element.
            if (ForceState::ForceComponentsVarScope* const fcp
                = m_state.tryGetForceComponents(nodep)) {
                const AstNodeDType* const dtypep = nodep->dtypep()->skipRefp();
                if (dtypep->isIntegralOrPacked() || VN_IS(dtypep, BasicDType)) {
                    // Packed/integral: replace VarRef directly with inline mux.
                    AstNodeExpr* const inlineExpr = fcp->forcedUpdate(nodep->varScopep());
                    nodep->replaceWith(inlineExpr);
                    VL_DO_DANGLING(nodep->deleteTree(), nodep);
                } else {
                    // Non-packed (unpacked array / struct): walk up to the outermost
                    // element-access expression (AstArraySel / AstStructSel), then replace
                    // the whole access with a mux whose three branches carry identical
                    // indexing.  This way the mux condition is the per-element enable, not
                    // the whole-array variable.  Stop walking as soon as the parent is not
                    // a selection node so we don't absorb surrounding operators.
                    AstNodeExpr* wholeExprp = nodep;
                    while (VN_IS(wholeExprp->backp(), ArraySel)
                           || VN_IS(wholeExprp->backp(), StructSel)) {
                        wholeExprp = VN_AS(wholeExprp->backp(), NodeExpr);
                    }
                    AstNodeExpr* const inlineExpr
                        = fcp->forcedUpdate(nodep->varScopep(), wholeExprp, nodep);
                    wholeExprp->replaceWith(inlineExpr);
                    // Defer deletion of detached subtree until after the visitor pass.
                    pushDeletep(wholeExprp);
                }
            }
            break;
        }
        case VAccess::WRITE: {
            if (!m_inLogic) return;
            // Emit valVscp update after each write to any VarRef on forced RHS.
            if (!m_state.getValVscps(nodep)) break;
            for (AstVarScope* const valVscp : *m_state.getValVscps(nodep)) {
                FileLine* const flp = nodep->fileline();
                AstAssign* assignp = m_state.getValVscpAssign(valVscp);
                UASSERT_OBJ(assignp, flp, "Missing stored assignment for forced valVscp");

                assignp = assignp->cloneTreePure(false);

                assignp->rhsp()->foreach(
                    [](AstVarRef* refp) { ForceState::markNonReplaceable(refp); });

                m_stmtp->addNextHere(assignp);
            }
            break;
        }
        default:
            if (!m_inLogic) return;
            if (m_state.tryGetForceComponents(nodep) || m_state.getValVscps(nodep)) {
                nodep->v3warn(
                    E_UNSUPPORTED,
                    "Unsupported: Signals used via read-write reference cannot be forced");
            }
            break;
        }
    }
    void visit(AstNode* nodep) override { iterateChildren(nodep); }

public:
    // CONSTRUCTOR
    explicit ForceReplaceVisitor(AstNetlist* nodep, const ForceState& state)
        : m_state{state} {
        iterateChildren(nodep);
    }
};
//######################################################################
//

void V3Force::forceAll(AstNetlist* nodep) {
    UINFO(2, __FUNCTION__ << ":");
    if (!v3Global.hasForceableSignals()) return;
    {
        ForceState state;
        { ForceConvertVisitor{nodep, state}; }
        { ForceReplaceVisitor{nodep, state}; }
        V3Global::dumpCheckGlobalTree("force", 0, dumpTreeEitherLevel() >= 3);
    }
}
