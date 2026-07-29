# Verilator Public Signals Optimization Analysis

## Executive Summary

This document reports the implementation of precise performance counters for public signal blocking in Verilator optimization passes. The work adds exact transformation-level instrumentation with sole-blocker attribution and deduplication across multiple pass invocations, enabling accurate performance analysis and regression detection.

---

## Precisely Instrumented Transformations

The following passes now provide exact performed/blocked counter pairs with sole-blocker attribution, deduplication, and comprehensive test coverage.

### 1. V3Dead: Dead Variable Elimination

**Counters:**
- `Optimizations, Dead variables eliminated` (performed)
- `Optimizations, Dead variables blocked by public` (blocked)

**Unit:** One dead variable elimination opportunity per compilation.

**Deduplication:** Blocked variables are deduplicated across all V3Dead invocations (deadifyModules, deadifyDTypes, deadifyAll, deadifyAllScoped) using pointer identity (`const AstVar*`). Each unique public variable is counted once per compilation. State is cleared at compilation start via `V3Dead::deadAllClear()`.

**Public Mode:** `isSigPublic()` blocks both `public_flat_rd` and `public_flat_rw`.

**Decision Point:** `src/V3Dead.cpp` - After verifying variable is unused (user1() == 0), checks `mightElimVarIgnoringPublic()` for non-public eligibility, then `isSigPublic()` as sole blocker.

**Sole-Blocker Rule:** Public check occurs after verifying the variable is actually dead (unused). A variable increments the blocked counter only when public visibility is the sole reason for retention.

**Tests:**
- `t_stats_dead_nonpublic.{v,py}` — Non-public baseline (1 eliminated, 0 blocked)
- `t_stats_dead_public.{v,py}` — Public variant (0 eliminated, 1 blocked)
- `t_stats_dead_public_used.{v,py}` — Independent rejection: used + public (0 eliminated, 0 blocked)

**Status:** ✅ Complete with passing tests and deduplication

---

### 2. V3Life: Lifetime Assignment Deletion

**Counters:**
- `Optimizations, Lifetime assign deletions` (performed)
- `Optimizations, Lifetime assign deletions blocked by public` (blocked)

**Unit:** One redundant assignment deletion opportunity per compilation.

**Deduplication:** Blocked assignments are deduplicated across both V3Life invocations using pointer identity (`const AstNodeStmt*`). Each unique assignment opportunity is counted once per compilation. State is cleared at compilation start via `V3Life::lifeAllClear()`.

**Public Mode:** `isSigPublic()` blocks both `public_flat_rd` and `public_flat_rw`.

**Decision Point:** `src/V3Life.cpp` - After verifying old assignment exists and checking non-public conditions (`isReadByDpi()`, `sensIfacep()`), checks `isSigPublic()` as sole blocker before deletion.

**Sole-Blocker Rule:** Public check occurs after all non-public eligibility conditions. An assignment increments the blocked counter only when public visibility is the sole reason for retention.

**Pass Behavior:** V3Life runs twice per compilation (lines 406 and 495 in Verilator.cpp). Deduplication ensures each unique assignment opportunity is counted once across both passes.

**Tests:**
- `t_stats_life_assn_nonpublic.{v,py}` — Non-public baseline (1 deleted, 0 blocked)
- `t_stats_life_assn_public.{v,py}` — Public variant (0 deleted, 1 blocked)
- `t_stats_life_assn_independent.{v,py}` — Independent rejection: DPI read + public (0 deleted, 0 blocked)

**Status:** ✅ Complete with passing tests and deduplication

---

### 3. V3Life: Lifetime Constant Propagation

**Counters:**
- `Optimizations, Lifetime constant prop` (performed)
- `Optimizations, Lifetime constant prop blocked by public` (blocked)

**Unit:** One constant propagation opportunity per compilation.

**Deduplication:** Blocked propagations are deduplicated across both V3Life invocations using pointer identity (`const AstVarRef*`). Each unique propagation opportunity is counted once per compilation. State is cleared at compilation start via `V3Life::lifeAllClear()`.

**Public Mode:** `isSigPublic()` blocks both `public_flat_rd` and `public_flat_rw`.

**Decision Point:** `src/V3Life.cpp` - After verifying constant value exists and checking non-public conditions (`isWrittenByDpi()`, `isVirtIface()`), checks `isSigPublic()` as sole blocker before replacement.

**Sole-Blocker Rule:** Public check occurs after all non-public eligibility conditions. A propagation increments the blocked counter only when public visibility is the sole reason for rejection.

**Pass Behavior:** Runs twice per compilation with deduplication ensuring each unique opportunity is counted once.

**Tests:**
- `t_stats_life_const_nonpublic.{v,py}` — Non-public baseline (1 propagated, 0 blocked)
- `t_stats_life_const_public.{v,py}` — Public variant (0 propagated, 1 blocked)
- `t_stats_life_const_independent.{v,py}` — Independent rejection: DPI write + public (0 propagated, 0 blocked)

**Status:** ✅ Complete with passing tests and deduplication

---

### 4. V3Localize: Variable Localization

**Counters:**
- `Optimizations, Vars localized` (performed)
- `Optimizations, Vars localization blocked by public` (blocked)

**Unit:** One variable localization opportunity per VarScope.

**Public Mode:** `isSigPublic()` blocks both `public_flat_rd` and `public_flat_rw`.

**Decision Point:** `src/V3Localize.cpp` - After verifying variable is optimizable, has single accessor, and is in leaf function, checks `isSigPublic()` as sole blocker.

**Sole-Blocker Rule:** Public check occurs after all non-public eligibility conditions (optimizability, accessor count, leaf-function requirement). A variable increments the blocked counter only when public visibility is the sole reason for rejection.

**Tests:**
- `t_stats_localize_nonpublic.{v,py}` — Non-public baseline (1 localized, 0 blocked)
- `t_stats_localize_public.{v,py}` — Public variant (0 localized, 1 blocked)
- `t_stats_localize_public_rd.{v,py}` — Read-only public (0 localized, 1 blocked)
- `t_stats_localize_string_public.{v,py}` — Independent rejection: string type + public (0 localized, 0 blocked)

**Status:** ✅ Complete with passing tests

---

### 5. V3Inline: Port Substitution

**Counters:**
- `Optimizations, Inline ports inlined` (performed)
- `Optimizations, Inline ports blocked by public_flat_rw` (blocked)

**Unit:** One port substitution per inlinable port.

**Public Mode:** Only `public_flat_rw` blocks (via `isSigUserRWPublic()`). Read-only public (`public_flat_rd`) does not block.

**Decision Point:** `src/V3Inline.cpp` - After verifying port is inlinable, checks `isSigUserRWPublic()` as sole blocker.

**Sole-Blocker Rule:** Public RW check occurs after all non-public eligibility conditions. A port increments the blocked counter only when RW public visibility is the sole reason for rejection.

**Tests:**
- `t_stats_inline_port_nonpublic.{v,py}` — Non-public baseline (1 inlined, 0 blocked)
- `t_stats_inline_port_public_rd.{v,py}` — Read-only public (1 inlined, 0 blocked)
- `t_stats_inline_port_public_rw.{v,py}` — Read-write public (0 inlined, 1 blocked)
- `t_stats_inline_port_forced.{v,py}` — Independent rejection: forced + public_rw (1 inlined, 0 blocked)

**Status:** ✅ Complete with passing tests

---

### 6. V3Unroll: Constant Binding Creation

**Counters:**
- `Optimizations, Const bindings created` (performed)
- `Optimizations, Const bindings blocked by public_flat_rw` (blocked)

**Unit:** One constant binding creation per unroll opportunity.

**Public Mode:** Only `public_flat_rw` blocks (via `isSigUserRWPublic()`). Read-only public (`public_flat_rd`) does not block.

**Decision Point:** `src/V3Unroll.cpp` - After verifying no existing binding and checking non-public conditions (`isForced()`), checks `isSigUserRWPublic()` as sole blocker.

**Sole-Blocker Rule:** Public RW check occurs after verifying no existing binding and checking forced status. A binding increments the blocked counter only when RW public visibility is the sole reason for rejection.

**Tests:**
- `t_stats_unroll_nonpublic.{v,py}` — Non-public baseline (1 created, 0 blocked)
- `t_stats_unroll_public_rd.{v,py}` — Read-only public (1 created, 0 blocked)
- `t_stats_unroll_public_rw.{v,py}` — Read-write public (0 created, 1 blocked)
- `t_stats_unroll_forced.{v,py}` — Independent rejection: forced + public_rw (1 created, 0 blocked)
- `t_stats_unroll_existing_binding.{v,py}` — Independent rejection: existing binding + public_rw (0 created, 0 blocked)

**Status:** ✅ Complete with passing tests

---

## Excluded Optimizations

The following passes were evaluated but excluded from precise instrumentation due to architectural constraints:

### V3Gate: Gate-Based Logic Elimination

**Reason for Exclusion:** V3Gate operates at the gate/logic level rather than variable level. Public signals affect gate optimization indirectly through connectivity and usage patterns, but there is no single decision point where public visibility is the sole blocker for a specific gate transformation. Attribution would require tracking complex multi-gate optimization chains.

**Current Behavior:** No public-specific counters. General gate optimization statistics remain available.

---

### V3Inline: Module Instance Inlining

**Reason for Exclusion:** Module inlining decisions are based on module-level properties (size, complexity, hierarchy) rather than individual signal visibility. Public signals within a module do not independently block module inlining.

**Current Behavior:** No public-specific counters. General module inlining statistics remain available.

---

### V3DfgOptimizer: Dataflow Graph Optimization

**Reason for Exclusion:** DFG optimization operates on dataflow graphs with complex multi-node transformations. While `hasExtWr` (derived from `isSigUserRWPublic()`) affects optimization decisions, there is no single transformation unit where public visibility is the sole blocker. Attribution would require tracking graph-level optimization chains.

**Current Behavior:** No public-specific counters. General DFG optimization statistics remain available.

---

## Implementation Details

### Deduplication Mechanism

**V3Dead:**
- Uses compilation-scoped global state (`DeadGlobalState`)
- Identity: pointer identity (`const AstVar*`)
- Cleared at compilation start via `V3Dead::deadAllClear()` in `Verilator.cpp`
- Persists across all deadify invocations in one compilation

**V3Life:**
- Uses compilation-scoped global state (`LifeGlobalState`)
- Assignment deletion identity: pointer identity (`const AstNodeStmt*`)
- Constant propagation identity: pointer identity (`const AstVarRef*`)
- Cleared at compilation start via `V3Life::lifeAllClear()` in `Verilator.cpp`
- Persists across both V3Life invocations (lines 406 and 495 in Verilator.cpp)

### Counter Naming Convention

All counter names follow the exact format used in source code:
- `Optimizations, Dead variables eliminated`
- `Optimizations, Dead variables blocked by public`
- `Optimizations, Lifetime assign deletions`
- `Optimizations, Lifetime assign deletions blocked by public`
- `Optimizations, Lifetime constant prop`
- `Optimizations, Lifetime constant prop blocked by public`
- `Optimizations, Vars localized`
- `Optimizations, Vars localization blocked by public`
- `Optimizations, Inline ports inlined`
- `Optimizations, Inline ports blocked by public_flat_rw`
- `Optimizations, Const bindings created`
- `Optimizations, Const bindings blocked by public_flat_rw`

### Test Coverage

All instrumented transformations have complete test coverage:
- Non-public baseline (performed > 0, blocked = 0)
- Public variant (performed = 0, blocked > 0)
- Independent rejection where applicable (performed = 0, blocked = 0)
- Multiple public modes where applicable (public_flat_rd vs public_flat_rw)

Total test files: 22 focused tests covering all six instrumented transformation pairs.

---

## Usage

### Viewing Statistics

Run Verilator with `--stats` flag:
```bash
verilator --stats <other_flags> <design.v>
```

Statistics are written to `<output_dir>/<prefix>__stats.txt`.

### Interpreting Results

For each instrumented transformation:
1. **Performed counter** = successful transformations
2. **Blocked counter** = opportunities blocked solely by public visibility
3. **Total opportunities** = performed + blocked (for equivalent designs)

### Performance Analysis

Compare non-public and public variants of the same design:
```
Non-public: performed = N, blocked = 0
Public:     performed = 0, blocked = N
```

The blocked count quantifies the optimization cost of public visibility.

---

## Limitations

1. **Deduplication scope:** Limited to single compilation. Cross-compilation analysis requires external aggregation.

2. **Independent rejection:** When multiple conditions block a transformation (e.g., public + DPI), neither counter increments. This is correct behavior for sole-blocker attribution.

3. **Compiler-generated variables:** Some passes (e.g., V3Dead) may count compiler-generated public variables (DPI infrastructure, timing support). This is expected and reflects actual blocking behavior.

4. **Pass-specific semantics:** Each pass defines its own transformation unit and counting rules. Cross-pass comparisons require understanding these semantics.

---

## Future Work

1. **Cross-compilation aggregation:** Tool to aggregate statistics across multiple compilations for large-scale analysis.

2. **Hierarchical attribution:** Break down blocked counts by module/hierarchy level for targeted optimization.

3. **Additional passes:** Evaluate other optimization passes for precise instrumentation feasibility.

4. **Performance impact:** Measure runtime overhead of deduplication and statistics collection.

---

## References

- Source files: `src/V3Dead.cpp`, `src/V3Life.cpp`, `src/V3Localize.cpp`, `src/V3Inline.cpp`, `src/V3Unroll.cpp`
- Test files: `test_regress/t/t_stats_*.{v,py}`
- Verilator documentation: `docs/guide/warnings.rst`