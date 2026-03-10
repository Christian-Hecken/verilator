# Implementation Notes: Elimination of `__VforceRd`

This document describes the implemented changes that eliminate the `__VforceRd` intermediate
signal from Verilator's force/release infrastructure, fixing
[issue #7092](https://github.com/verilator/verilator/issues/7092).  It should be read alongside
the companion design document `internals_eliminate_VforceRd.md`, which contains the original
proposal.  This file focuses on *what was actually built*, the key design decisions made during
implementation, and the ways in which the implementation differs from the initial proposal.

---

## Overview of What Changed

| File | Nature of change |
|---|---|
| `src/V3Force.cpp` | Primary implementation: reworked `ForceComponentsVar`, `ForceComponentsVarScope`, and both visitor classes |
| `src/V3EmitCSyms.cpp` | Removed `"__VforceRd"` from the force-control suffix filter list |
| `include/verilated_vpi.cpp` | Comment-only update: clarified that `__VforceRd` no longer exists |
| `test_regress/t/t_forceable_public_flat.py` | Removed stale grep check for `__VforceRd` in generated headers |

---

## Architecture as Implemented

### Invariant: two force-control signals per forceable scope

The implementation keeps exactly two auxiliary signals per forceable `AstVarScope`:

- `<name>__VforceEn` (`VVarType::VAR`): per-bit enable bitmask (packed) or per-element/per-member
  boolean (unpacked array or struct), zero-initialised in a static `initial` block.
- `<name>__VforceVal` (`VVarType::VAR`): the value currently being forced, with the same dtype as
  the original signal; its initial value is don't-care.

The third signal `<name>__VforceRd` is gone entirely.

### Read references: inline mux at each read site

Every read reference to a forceable signal `<name>` is replaced in `ForceReplaceVisitor` with an
inline mux expression built by `ForceComponentsVarScope::forcedUpdate()`:

- **Packed / integral types**: `AstCond { en, val, orig }` or, for multi-bit signals,
  `AstOr { AstAnd{en, val}, AstAnd{ AstNot{en}, orig } }`.
- **Non-packed types** (unpacked arrays, structs): the `AstVarRef` is first walked up through
  `AstArraySel` / `AstStructSel` parent nodes to find the full element-access expression
  `wholeExprp`, which is then replaced by `forcedUpdate(vscp, wholeExprp, nodep)`.  The three
  branches of the mux carry independent clones of the element-access expression each substituting
  a different leaf (`__VforceEn`, `__VforceVal`, or the original signal VarRef).

Because the mux is evaluated on every read, reads are always current regardless of whether an
`eval()` cycle has been completed since the last VPI write — this is the fix for issue #7092.

### Force (`AstAssignForce`) lowering

Lowered to two assignments only:

1. `<lhs>__VforceEn = 1` (or all-ones for multi-bit).
2. `<lhs>__VforceVal = <rhs>`.

The third assignment `<lhs>__VforceRd = <rhs>` from the old scheme is removed.  It was an eager
in-process cache update; the cache no longer exists.

### Release (`AstRelease`) lowering

Split by signal kind:

| Kind | Emitted statements |
|---|---|
| **Net** (`isContinuously()`) | `<lhs>__VforceEn = 0` only |
| **Variable** (procedural) | `<lhs> = forcedUpdate(vscp, lhsp, refp)` then `<lhs>__VforceEn = 0` |

For nets, the inline mux at each read site reverts automatically once `__VforceEn` is zero, so no
explicit `__VforceRd` reset is needed.

For variables, the variable retains its forced value at release time per IEEE 1800-2023 §10.6.2.
The assignment uses `forcedUpdate()` with the original `lhsp` (element-access or bare VarRef) as
the template expression, so that element-level releases (e.g. `release arr[i][j]`) write back
only the specific element rather than the whole array.

---

## Key Implementation Decisions

### 1. `genEnZeroInitStmtsRecursep`: recursive zero-init without a template

The proposal suggested cloning the "update" statements and patching them for zero-init.  In practice
the update statements no longer exist, so a dedicated recursive helper
`genEnZeroInitStmtsRecursep(AstNodeExpr* lhsp)` was written instead.  It handles three cases:

- **Packed / integral**: emits `AstAssign{ lhsp, AstConst{0} }` directly.
- **Struct**: iterates `AstMemberDType` children, recurses per member using `AstStructSel`.
- **Unpacked array**: generates a loop with a fresh `AstVar` loop-counter (named via
  `m_iterNames`), an `AstLoop` / `AstLoopTest` / `AstArraySel` construct, and recursion into
  elements.  This avoids quadratic expansion for large arrays.

The `m_iterNames` field (`V3UniqueNames`) is therefore retained in `ForceComponentsVarScope`
despite the proposal suggesting its removal; it is still required for naming loop-counter
variables inside `genEnZeroInitStmtsRecursep`.

### 2. Non-packed READ: walk up only through selector nodes

The proposal sketched the non-packed READ replacement as a walk-up through `NodeExpr` ancestors.
The actual implementation restricts the walk to `AstArraySel` and `AstStructSel` parents:

```cpp
while (VN_IS(wholeExprp->backp(), ArraySel)
       || VN_IS(wholeExprp->backp(), StructSel)) {
    wholeExprp = VN_AS(wholeExprp->backp(), NodeExpr);
}
```

Walking through *all* `NodeExpr` parents would incorrectly absorb surrounding operators (e.g.
`arr[i] !== 1` would make the whole comparison the "element access"), producing an expression
that has the wrong dtype for the mux and breaking V3Broken integrity checks.

### 3. Non-packed READ: deferred deletion via `pushDeletep`

After `wholeExprp->replaceWith(inlineExpr)`, the detached `wholeExprp` subtree (which still
contains `nodep`, the `AstVarRef` that triggered the visit) must be freed.  `deleteTree()` cannot
be called immediately because `iterateAndNext` is still on the call stack with a live pointer to
`nodep`.  The standard Verilator pattern `pushDeletep(wholeExprp)` is used to defer deletion
until after the visitor pass completes:

```cpp
wholeExprp->replaceWith(inlineExpr);
pushDeletep(wholeExprp);  // safe deferred deletion
```

### 4. Element-level release for unpacked arrays

The proposal described the variable release path as simply `<name> = forcedUpdate()` (whole-variable
form).  For unpacked arrays the whole-array `forcedUpdate()` returns a whole-array `AstCond` whose
condition is the entire `__VforceEn` array, which is not convertible to `bool` and fails C++
compilation.

The fix is to pass `lhsp` (the element-access expression detached from the `AstRelease` node) and
`refp` (the innermost `AstVarRef` within it) to `forcedUpdate()`, which uses `wrapIntoExprp()` to
clone the element-access three times and substitute the appropriate leaf in each branch.  When
the LHS is already a bare `AstVarRef` (`lhsp == refp`), the original whole-variable form is used:

```cpp
AstNodeExpr* const rhsp = (lhsp == refp) ? fcp.forcedUpdate(vscp)
                                          : fcp.forcedUpdate(vscp, lhsp, refp);
```

---

## Differences from the Initial Proposal

| Topic | Proposal | Implementation |
|---|---|---|
| `m_iterNames` removal | Suggested removing `m_iterNames` along with `getForcedUpdateStmtsRecursep` | **Retained**: still needed by the new `genEnZeroInitStmtsRecursep` for loop-counter names in unpacked-array init |
| Non-packed READ walk-up | "walk up through `NodeExpr` ancestors" | **Restricted to `AstArraySel` / `AstStructSel`** to avoid absorbing surrounding operators |
| Deletion of replaced node | Implicit (not discussed) | **`pushDeletep`** required for AST integrity; immediate `deleteTree` would dangle a pointer still on the `iterateAndNext` call stack |
| Variable release RHS | `<name> = forcedUpdate()` (whole-variable) | **Element-aware**: uses `forcedUpdate(vscp, lhsp, refp)` for element-level releases; falls back to whole-variable form only when `lhsp == refp` |
| `genEnZeroInitStmtsRecursep` | Not described (implied: clone-and-patch from update stmts) | **New recursive helper** introduced; handles packed, struct, and unpacked array cases independently |
| READ packed path | Described as `nodep->replaceWith(...); VL_DO_DANGLING(nodep->deleteTree(), nodep)` | Identical |
| Net release path | `__VforceEn = 0` only | Identical |
| `V3EmitCSyms.cpp` | Remove `"__VforceRd"` from suffix list | Identical |
| `verilated_vpi.cpp` | Comment update only | Identical |
| Test update | Remove `__VforceRd` public-header grep | Identical |

---

## Files Not Changed (as predicted)

The following files were confirmed to require no changes, as predicted in the proposal:

- `include/verilated_sym_props.h`
- `src/V3AstNodeOther.h`
- `src/V3AstNodes.cpp`
- `src/V3SplitVar.cpp`
- `src/V3LinkParse.cpp`, `src/V3Control.cpp`
- `src/V3Global.h`
