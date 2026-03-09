# Dead Public Signal Optimization — Architecture & Implementation

## Overview

When `--public-flat-rw` is active, Verilator marks every signal as public,
which blocks most optimization passes from eliminating dead code.  This
commit introduces three targeted changes that recover performance without
breaking VPI correctness or ABI stability.

See `PROPOSAL_dead_signal_optimization.md` for the full analysis of root
causes (Sections 3.A–3.G) and the originally proposed optimization plan
(Phases 1–4).

---

## Changes Implemented

### 1. `GateDeadPublicReduction` — Dead Public Signal Elimination

**File:** `src/V3Gate.cpp`

**What it does:** A new pass that runs after `GateTrivialAliasReduction` and
before `GateInline`.  It scans the gate graph for public variable vertices
that have **zero outgoing edges** (no internal consumers).  For each such
vertex it calls `setReducible("DeadPublic")`, which allows `GateInline` to
process it.  Because there are no consumers, `GateInline` finds nothing to
substitute into, which removes the edge from the driving logic vertex to the
variable vertex.  The driving logic vertex then has no downstream consumers
and is deleted by `GateUnused`.

**Net effect:** The eval-time assignment (`assign dead_tmp = in ^ i[0]`) is
eliminated from the fast-path C++ code.  The struct member persists (V3Dead
still sees `isSigPublic()` and keeps it), preserving ABI.  The `varInsert`
VPI registration also persists (the struct member has a valid address), so
VPI reads return the initial/reset value.

**Guard:** Only runs when `v3Global.opt.publicFlatRW()` is true, matching the
same guard used by `GateTrivialAliasReduction`.

**Statistics:** Reports `"Optimizations, Gate sigs dead-reduced (public)"`.

**Corresponds to:** Proposal Phase 1.  Implemented as proposed, no deviations.

---

### 2. Tighten RD/RW Distinction in V3Life

**File:** `src/V3Life.cpp`

**What changed:** Two `isSigPublic()` guards were narrowed to
`isSigUserRWPublic()`:

- `checkRemoveAssign()` (line ~130): Controls whether redundant (overwritten)
  assignments can be removed.  Read-only public signals are never written
  externally, so it is safe to remove an assignment that is immediately
  overwritten by another.

- `varUsageReplace()` (line ~175): Controls whether a variable reference can
  be replaced with a known constant value.  Same reasoning — a read-only
  public signal's value is never modified from outside, so constant
  substitution is valid.

**Corresponds to:** Proposal Phase 2, but **only the V3Life half**.

#### Deviation from Proposal

The proposal also suggested tightening V3Gate's `isSigPublic()` check (in
`GateGraph::makeVarVertex`) to `isSigUserRWPublic()`.  This was implemented
and tested but **reverted** because it broke `t_dpi_var`:

- **Root cause:** Gate reduction inlines a variable's driving expression into
  every consumer site and can then eliminate the variable's physical storage
  entirely.  For a read-only public signal, external code (DPI, VPI, or
  direct C++ struct access) holds a pointer to that storage and reads from
  it.  If the storage is eliminated, those reads return stale/invalid data.

- **Why V3Life is safe but V3Gate is not:** V3Life only removes *redundant
  assignments* (an assignment immediately overwritten) and substitutes
  *constant values* into read sites.  Neither operation removes the
  variable's storage — the variable and its final assignment survive.  V3Gate
  reduction, by contrast, can eliminate the variable entirely.

- **Lesson:** `isSigPublic()` in V3Gate protects storage stability, not just
  value correctness.  Any future change here requires ensuring the variable's
  struct member is preserved and kept up-to-date.

---

### 3. Activity-Based Trace for Read-Only Public Signals

**File:** `src/V3Trace.cpp`

**What changed:** In `visit(AstVarRef*)`, the always-active trace edge
creation condition was narrowed from `isSigPublic()` to
`isSigUserRWPublic()`.

**Before:** Every public signal (RD or RW) got an always-active edge in the
trace graph, meaning the trace system checked for changes on every dump
regardless of whether any writing code executed.

**After:** Only read-write public signals get the always-active edge.
Read-only public signals use normal activity-based tracing: they are only
checked when the code that writes them actually executes.  This is correct
because read-only public signals are never written externally — the only
source of changes is internal code, which is already tracked by the activity
system.

**Corresponds to:** Proposal Phase 4.  Implemented as proposed, no
deviations.

---

## Not Implemented

### Phase 3: Guard ICO Region with `evalNeeded`

**Proposal:** Insert a runtime guard at the top of the ICO (Input
Combinational) loop to skip it when no `vpi_put_value` has occurred since
the last `eval()`.

**Why deferred:** The ICO loop's trigger logic mixes primary-input triggers
and public-signal triggers onto the same `inputChanged` sentinel
(`V3Sched.cpp`, line ~546).  A blanket `evalNeeded` guard would skip the
entire ICO region — including primary-input-driven combinational logic that
must always execute.  A correct fix requires separating the triggers
(Proposal Section 5.2, Option C), which is a deeper architectural change to
the scheduling infrastructure.

---

## Test Results

All changes were validated against the full relevant test suites with zero
regressions:

| Suite | Tests Run | Result |
|---|---|---|
| `t_dead_elim*` | All | PASSED |
| `t_gate*` | All (excluding pre-existing `_bad`/`_unsup` failures) | PASSED |
| `t_vpi*` | All | PASSED |
| `t_trace*` | All (excluding pre-existing `t_trace_multi_bad`) | PASSED |
| `t_dpi_var` | 1 | PASSED |
| `t_aliasing_public` | 1 | PASSED |
| `t_hier_block` | 1 | PASSED |
| `t_inst_tree_inl{0,1}_pub1` | 2 | PASSED |
| `t_inline*` | All | PASSED |
| `t_life*` | All | PASSED |
| `t_export_packed_struct2` | 1 | PASSED |

### Verification of `t_dead_elim_public`

| Artifact | `dead_tmp` count | Notes |
|---|---|---|
| `___024root.h` (struct) | 4096 | Struct members preserved (ABI) |
| `___024root__0.cpp` (eval) | **0** | All eval assignments eliminated |
| `__Syms__ctor__*.cpp` (VPI) | ~4096 | `varInsert` registrations preserved |
| `___024root__0__Slow.cpp` (init) | 4096 | One-time randomization init preserved |
