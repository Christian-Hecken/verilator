# Architectural Change: Eliminate `__VforceRd` Signal

## Background

**Issue:** [#7092](https://github.com/verilator/verilator/issues/7092) — Forceable signals have incorrect value immediately after `vpi_put_value` with `vpiNoDelay`

When a signal is marked `/*verilator forceable*/`, Verilator currently generates a
`__VforceRd` intermediate wire that is computed once per `eval()` cycle. SystemVerilog
code that reads the signal actually reads `__VforceRd` rather than the signal itself.
After a `vpi_put_value(vpiNoDelay)` writes the base signal between two `eval()` calls,
`__VforceRd` retains its previous (stale) value. This causes forceable signals to behave
differently from non-forceable signals when read immediately after a VPI write.

---

## Current Architecture

For each forceable signal `<name>`, `V3Force.cpp` currently:

1. **Creates three auxiliary signals:**
   - `<name>__VforceRd` (WIRE): cached result of the force mux
   - `<name>__VforceEn` (VAR): per-bit force-enable bitmask
   - `<name>__VforceVal` (VAR): the value being forced

2. **Emits one initial block** to zero-initialise `__VforceEn`.

3. **Emits one combinational `always` block** to keep `__VforceRd` up to date:
   ```sv
   always @(<name>, <name>__VforceEn, <name>__VforceVal, <name>__VforceRd)
       <name>__VforceRd = <name>__VforceEn ? <name>__VforceVal : <name>;
   ```

4. **Replaces all read-references** to `<name>` with references to `<name>__VforceRd`.

5. For `force <name> = rhs`:
   - Assigns `__VforceEn = 1`
   - Assigns `__VforceVal = rhs`
   - Assigns `__VforceRd = rhs` (eager cache update within the same process)

6. For `release <name>`:
   - For nets (continuously driven): assigns `__VforceRd = <name>` then `__VforceEn = 0`
   - For variables (procedural): assigns `<name> = forcedUpdate()` then `__VforceEn = 0`

7. **After every write to `<name>`** in a procedural context, emits an eager update:
   ```sv
   <name>__VforceRd = <name>__VforceEn ? <name>__VforceVal : <name>;
   ```

The root cause of issue #7092 is step 4: once `__VforceRd` has been computed during
an `eval()`, it holds a stale value until the next `eval()`, even if `<name>` is
written externally through VPI between calls.

---

## Proposed Architecture

**Eliminate `__VforceRd` as a stored signal.** Replace every read reference to
`<name>` with the inline mux expression directly:

```cpp
// For packed/integral types:
<name>__VforceEn ? <name>__VforceVal : <name>
// For ranged (multi-bit) types, bitwise form:
(<name>__VforceEn & <name>__VforceVal) | (~<name>__VforceEn & <name>)
```

Because the mux is now evaluated on every read rather than cached in a stored wire,
it reflects the current state of `<name>`, `__VforceEn`, and `__VforceVal` at
read time — without requiring an `eval()`. This eliminates the staleness bug.

The `forcedUpdate()` method in `ForceComponentsVarScope` already builds exactly this
expression tree (`AstCond` or `AstOr`/`AstAnd`/`AstNot`). It is reused in the new
approach.

---

## Required Changes

### `src/V3Force.cpp`

This is the primary file to modify.

#### 1. `ForceComponentsVar` struct (lines ~61–75)

- **Remove** the `m_rdVarp` field (`AstVar* const m_rdVarp`).
- **Remove** the construction of the `__VforceRd` `AstVar` (the `new AstVar{..., "__VforceRd", ...}` call).
- **Remove** the `addNextHere` calls that insert `m_rdVarp` into the module's variable list.
- Keep `m_valVarp` and `m_enVarp` unchanged.

#### 2. `ForceComponentsVarScope` struct (lines ~79–143)

This struct has the most changes.

- **Remove** the `m_rdVscp` field (`AstVarScope* const m_rdVscp`).
- **Remove** the construction of the `__VforceRd` `AstVarScope`.
- **Remove** the `m_iterNames` field (it was only used to name loop-counter variables inside
  `getForcedUpdateStmtsRecursep`).
- **Remove** the entire "force-update" `AstActive` block — the combinational `always` that
  continuously recomputes `__VforceRd`. This removes the sensitivity list entries for
  `m_rdVscp`, `m_valVscp`, `m_enVscp`, and the original signal.
- **Remove** `getForcedUpdateStmtsRecursep()` entirely — it was only used to (a) build
  `rdUpdateStmtsp` for the now-removed always block, and (b) build `enInitStmtsp` by
  cloning and patching `rdUpdateStmtsp`.
- **Change** the `__VforceEn` zero-initialisation to build the initial assignment
  directly without going through the clone-and-patch path:
  - For packed/integral types, construct `AstInitial{ AstAssign{ ref to m_enVscp, AstConst{0} } }`.
  - For struct or unpacked-array `__VforceEn` types, build the equivalent per-member or
    per-element loop directly, without needing `rdUpdateStmtsp` as a template.
- Keep `forcedUpdate()` and `wrapIntoExprp()` — they are still needed for the inline
  read-site expansion and for the non-net variable release.

#### 3. `ForceConvertVisitor::visit(AstAssignForce*)` (lines ~465–507)

- **Remove** the `setRdp` assignment (the third assignment that wrote `__VforceRd = rhs`
  for eager in-process cache update).
- Keep `setEnp` (`__VforceEn = 1`) and `setValp` (`__VforceVal = rhs`) unchanged.

#### 4. `ForceConvertVisitor::visit(AstRelease*)` (lines ~509–570)

Split by signal kind:

| Signal kind | Current behaviour | New behaviour |
|---|---|---|
| **Net** (continuously driven, `isContinuously()`) | `__VforceRd = <name>` then `__VforceEn = 0` | Only `__VforceEn = 0` |
| **Variable** (procedural) | `<name> = forcedUpdate()` then `__VforceEn = 0` | `<name> = __VforceVal` (or equivalent `forcedUpdate()` while `__VforceEn` is still 1) then `__VforceEn = 0` |

For nets, remove `resetRdp` entirely — the inline mux expression at each read site
automatically yields the net's current driven value once `__VforceEn` is zero.

For variables, keep the assignment that writes the forced value back into `<name>` before
clearing `__VforceEn`. This satisfies IEEE 1800-2023 §10.6.2: after release, a variable
that is not driven by a continuous assignment retains its value at the time of release.
The LHS target is already `<name>` (not `__VforceRd`) in the current non-net path, so
only the net path requires a structural simplification.

#### 5. `ForceReplaceVisitor::visit(AstVarRef*)` — READ case (lines ~644–650)

This is the core semantic change.

**Before:**
```cpp
// Replace VarRef from forced LHS with rdVscp.
nodep->varp(fcp->m_rdVscp->varp());
nodep->varScopep(fcp->m_rdVscp);
```

**After:**
```cpp
// Replace VarRef with inline mux expression.
AstNodeExpr* const inlineExpr = fcp->forcedUpdate(nodep->varScopep());
nodep->replaceWith(inlineExpr);
VL_DO_DANGLING(nodep->deleteTree(), nodep);
```

The `AstVarRef` node itself is replaced (not simply mutated) because `forcedUpdate()`
returns a new `AstCond`/`AstOr` expression tree rather than a `VarRef`.
The `AstVarRef` to the original signal inside `inlineExpr` is marked non-replaceable
(via `ForceState::markNonReplaceable`) so the visitor will not attempt to expand it
recursively.

#### 6. `ForceReplaceVisitor::visit(AstVarRef*)` — WRITE case (lines ~652–682)

- **Remove** the block that emits `__VforceRd = forcedUpdate(...)` after each procedural
  write. This block is no longer needed because there is no `__VforceRd` to update.
- Keep the block that updates `__VforceVal` after writes to signals on a forced RHS
  (the `m_state.getValVscps(nodep)` path). This is unrelated to `__VforceRd`.

#### 7. Comment/documentation update (lines ~17–42)

Update the transformation description at the top of the file to reflect the new scheme:

- The entry for `<name>__VforceRd` and its `assign` statement should be removed.
- The "replace all READ references" entry becomes "replace all read references to
  `<name>` with the inline expression `__VforceEn ? __VforceVal : <name>`".
- Remove entries for `<lhs>__VforceRd = <rhs>`, `<lhs>__VforceRd = <lhs> // iff lhs is a net`,
  and "reevaluate `<lhs>__VforceRd` to support immediate force/release".

---

### `src/V3EmitCSyms.cpp`

#### `isForceControlSignal()` helper (line ~212)

Remove `"__VforceRd"` from the loop that checks whether a variable name ends with a
force-control suffix. After this change, `__VforceRd` variables will no longer be
emitted, so there is nothing to filter.

```cpp
// Before:
for (const std::string forceControlSuffix : {"__VforceEn", "__VforceVal", "__VforceRd"}) {

// After:
for (const std::string forceControlSuffix : {"__VforceEn", "__VforceVal"}) {
```

---

### `include/verilated_vpi.cpp`

No functional changes are required. Two comments (around lines 1149 and 2879) explain
why the VPI code manually reconstructs the mux from `__VforceEn` and `__VforceVal`
rather than reading `__VforceRd` directly (since `__VforceRd` is not public). After the
change, these comments should be updated to reflect that `__VforceRd` no longer exists
and the VPI approach is now the same as the compiler-internal approach.

---

## Files Not Requiring Changes

| File | Reason |
|---|---|
| `include/verilated_sym_props.h` | Already has only `forceEnableSignalp` and `forceValueSignalp`; no `forceRdSignalp` |
| `src/V3AstNodeOther.h` | `isForceable()` attribute and `m_isForceable` bit are unaffected |
| `src/V3AstNodes.cpp` | `VLVF_FORCEABLE` flag emission is unaffected |
| `src/V3SplitVar.cpp` | Guard preventing split of forceable variables is unaffected |
| `src/V3LinkParse.cpp`, `src/V3Control.cpp` | `setForceable()` / `setHasForceableSignals()` infrastructure is unaffected |
| `src/V3Global.h` | `hasForceableSignals()` flag is unaffected |

---

## Performance Considerations

| Aspect | Current | After change |
|---|---|---|
| Memory | One extra `__VforceRd` word per forceable signal per scope | Eliminated |
| Simulation overhead (not forced) | Combinational always block reschedules when `<name>` changes | Inline branch at each read site; branch predictor handles `__VforceEn == 0` well |
| Simulation overhead (forced) | `__VforceRd` always block fires once; reads are cheap loads | Mux evaluated at each read site; may be duplicated if the signal has many readers |
| `eval()` exit | Combinational always contributes one activity check per forceable signal | Eliminated |

For designs with many read sites and a signal that is rarely (or never) actually forced,
the branch at each read site (`__VforceEn ? __VforceVal : <name>`) is essentially
zero-cost because the branch predictor will learn that `__VforceEn` is zero. For designs
that force a signal that is read in a tight hot loop, inlining may increase code size
slightly, but the mux is a single conditional-move or bitwise operation on most targets.

---

## Test Coverage

The existing regression test `test_regress/t/t_vpi_var.v` — modified to mark `onebit`
as `/*verilator forceable*/` per issue #7092 — serves as the primary regression test.
A dedicated test case for the specific bug scenario (VPI `vpiNoDelay` write followed by
immediate SV read of a forceable signal without an intervening `eval()`) should be added.

The file `test_regress/t/t_ghost_perf.v` (in the `lazy_force_read` branch) provides a
microbenchmark to confirm that the inlining approach does not introduce a measurable
performance regression.

---

## Summary of Node-Level Changes in the AST

| AST construct | Before | After |
|---|---|---|
| `AstVar` `__VforceRd` | Created for every forceable `AstVar` | **Removed** |
| `AstVarScope` `__VforceRd` | Created for every forceable `AstVarScope` | **Removed** |
| `AstAlways` "force-update" | One per forceable scope (combinational update block) | **Removed** |
| `AstAssign` `__VforceRd = rhs` inside `AstAssignForce` lowering | Emitted (third of three assigns) | **Removed** |
| `AstAssign` `__VforceRd = <name>` inside net `AstRelease` lowering | Emitted | **Removed** |
| `AstAssign` `__VforceRd = forcedUpdate()` after each procedural write | Emitted by `ForceReplaceVisitor` | **Removed** |
| READ `AstVarRef` to forceable signal | Mutated to point at `__VforceRd` | **Replaced** with `AstCond`/`AstOr` inline expression |
