# Proposal: Dead Public Signal Elimination & Related Optimizations

## Problem Statement

The file `test_regress/obj_vlt/t_dead_elim_public/Vt_dead_elim_public___024root.h`
contains **hundreds of `DEAD` signals** (e.g., `t__DOT__DEAD__BRA__1__KET____DOT__dead_tmp`
through index 4095) that are never read by the design — they are purely dead code.
They survive only because `--public-flat-rw` marks every signal as public, and
Verilator's optimization passes treat public signals as untouchable.

The `thesis_proposal.md` identifies **seven root causes** (3.A–3.G) and **six
optimization opportunities** (5.1–5.6). This document proposes a phased
implementation plan, ordered by effort-to-impact ratio.

---

## Phase 1: Extend `GateTrivialAliasReduction` to Handle Dead Signals

**Impact:** HIGH — builds on existing work.

The workspace already has a working `GateTrivialAliasReduction` pass in
`src/V3Gate.cpp` that eliminates trivial `assign A = B` aliases. The dead
signals in the test are **not** simple aliases — they are
`assign dead_tmp = in ^ i[0]` (expressions, not bare `AstVarRef`). The
existing pass skips them.

### Approach

Add a new reduction category — **"dead public signal elimination"** — that
detects public signals with **zero consumers** (no downstream fanout other
than VPI registration). These signals' eval logic can be removed entirely
since nobody reads the value, and the `varInsert` registration can either be
dropped or kept pointing at a dummy.

### Key Changes

- In the gate graph scan (after `GateInline`), identify public variable
  vertices with **no outgoing consumption edges** (only the
  `setConsumed("SigPublic")` flag keeps them alive).
- For such signals, remove the `setConsumed("SigPublic")` marking, allowing
  `GateInline` to eliminate the assignment.
- Keep the struct member for ABI stability but skip the `varInsert` call (or
  register a zeroed-out dummy).
- Guard behind `--public-flat-rw` (same as existing alias optimization).

### Why This Works

All 4096 `dead_tmp` signals have exactly **zero** readers — their `assign` is
the only statement touching them. The gate graph will show no consumption
edges, making detection trivial.

---

## Phase 2: Tighten RD/RW Distinction in V3Life

**Impact:** MEDIUM. **Effort:** LOW.

V3Life uses the overly broad `isSigPublic()` check when it should use
`isSigUserRWPublic()`:

**V3Life** (`src/V3Life.cpp`) — `isSigPublic()` blocks constant propagation
and dead assignment removal. Fix: only block for `isSigUserRWPublic()`.
Read-only public signals are never externally written, so redundant
assignment removal and constant substitution are safe.

**Note on V3Gate:** The thesis proposal (Section 5.3) suggested also
tightening V3Gate's `isSigPublic()` to `isSigUserRWPublic()`. Testing
revealed this is NOT safe: gate reduction inlines a variable into its
consumers and can orphan the variable's storage, making external reads
(via DPI, VPI, or direct C++ pointers) return stale values. Read-only
public signals still need stable, up-to-date storage values. V3Gate's
check must remain `isSigPublic()`.

### Why This Matters Separately from Phase 1

Phase 1 handles signals with zero consumers. Phase 2 handles signals that
*are* consumed but could still be simplified through constant folding — a
different (larger) class of signals in real designs.

---

## Phase 3: Guard ICO Region with `evalNeeded`

**Impact:** HIGH. **Effort:** LOW-MEDIUM.

All combinational logic downstream of `public_flat_rw` signals is cloned into
the ICO region and re-evaluated **every `eval()` call**. The simplest fix:

In `src/V3Sched.cpp`, emit a guard at the top of the ICO loop:
```cpp
if (!VerilatedVpi::evalNeeded()) goto skip_ico;
```

This skips the entire ICO region when no `vpi_put_value` has occurred since
the last `eval()` — which is the common case in most testbenches. The
`evalNeeded` flag infrastructure already exists.

**Risk:** Low — the guard is a simple boolean check. The flag is already
correctly set by `vpi_put_value` and cleared by `eval()`.

---

## Phase 4: Activity-Based Trace for Public Signals

**Impact:** MEDIUM. **Effort:** LOW.

In `src/V3Trace.cpp` line 922, all public signals get an "always active" trace
edge. Fix:

- For `public_flat_rd` signals: use normal activity-based tracing (the writing
  code is internal, so activity tracking works).
- For `public_flat_rw` signals: tie to the `evalNeeded` flag rather than
  always-active.

---

## Implementation Summary

| Phase | Files Modified | Test Coverage | Risk |
|---|---|---|---|
| 1 | `src/V3Gate.cpp`, `src/V3EmitCSyms.cpp` | `t_dead_elim_public` (existing) | Low |
| 2 | `src/V3Life.cpp` | Existing regression suite | Low |
| 3 | `src/V3Sched.cpp`, `include/verilated.cpp` | VPI write tests | Medium |
| 4 | `src/V3Trace.cpp` | `t_trace_public*` tests | Low |

### Validation for Each Phase

1. Run the full regression suite (`make test`).
2. Verify `t_dead_elim_public` — the 4096 dead signals should be eliminated
   from eval logic (check `--stats` output for reduced gate counts).
3. Verify VPI correctness with existing `t_vpi_*` tests.
4. Measure `eval()` throughput before/after on holistic benchmarks.

### Recommended Starting Order

**Phase 2** first — lowest effort (two `isSigPublic()` → `isSigUserRWPublic()`
substitutions), quick win, lowest risk.

**Phase 1** next — biggest payoff for the specific dead-signal problem,
extends a proven pattern.

**Phase 3** and **Phase 4** follow as independent improvements.
