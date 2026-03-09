# VPI Alias Optimization — Architecture & Design Notes

## Overview

Public signals annotated with `public_flat_rw` / `public_flat_rd` are normally
blocked from gate-level optimization because VPI registration requires a stable
address for each signal. In many designs the vast majority of these signals are
trivial aliases — simple `assign A = B` wires — that occupy struct storage
solely to satisfy VPI. This optimization detects those aliases, eliminates
their struct storage, and redirects VPI registration to the canonical
variable's address.

Result on the reference design (`t_aliasing_public`): **4 115 → 19** struct
members.

---

## Architecture

The implementation spans four files and three pipeline stages:

```
V3Gate  ──────────────────────────────┐
│                                     │
│  1. GateTrivialAliasReduction       │  Detect  assign A = B  patterns.
│     (before GateInline)             │  Set  vpiAlias(B)  on  A.
│                                     │  Re-enable gate reduction.
│                                     │
│  2. GateInline::apply()             │  Inline/substitute the var.
│                                     │
│  3. Post-inline storage scan        │  Mark unreferenced alias vars
│     (after GateInline)              │  with  noCReset(true).
└─────────────────────────────────────┘
          ↓
V3Dead  ──  deletes unreferenced non-public vars
V3CCtors ──  skips  noCReset  vars (no reset code generated)
          ↓
V3EmitCHeaders  ──  skips struct emission for  vpiAlias && noCReset  vars
V3EmitCSyms     ──  redirects  varInsert()  address to alias target
```

### 1. `GateTrivialAliasReduction` (src/V3Gate.cpp)

Runs **before** `GateInline`. Scans the gate graph for public variables that
were blocked from reduction. For each candidate:

1. Must have exactly **one driver** (single incoming edge).
2. The driver must be a simple assignment whose RHS is a **pure `AstVarRef`**
   (no expressions, no concatenations — just `assign A = B`).
3. The driver variable (`B`) must be **public or IO** (see _Pitfall 1_ below).

When matched, `vpiAlias(driverVarp)` is set on the source variable and its
graph vertex is re-enabled for substitution (`setReducible("TrivialAlias")`).

### 2. Post-Inline Storage Elimination Scan (src/V3Gate.cpp)

Runs **after** `GateInline`. At this point the alias vars have been substituted
out of all eval logic, but they still exist in the AST as `AstVar` nodes.
The scan:

1. Collects all `AstVar` nodes referenced by any `AstNodeVarRef` in the design.
2. Walks all `vpiAlias` chains and inserts the canonical target into the
   referenced set (so the target retains storage for VPI address redirection).
3. Any `vpiAlias` var that is **not IO** and has **zero remaining references**
   is marked `noCReset(true)`.

`noCReset` was chosen because it:
- Already exists on `AstVar` and is checked by `V3CCtors` (no reset code).
- Doesn't interfere with other passes.
- Provides a clear predicate for downstream emission filters.

### 3. Address Redirection in `V3EmitCSyms` (src/V3EmitCSyms.cpp)

During `visit(AstVar*)`, alias chains are followed to build `m_aliasMap` (source → canonical target).

When emitting `varInsert()` calls, the alias target's name is used in the
address expression **only when `noCReset()` is true** — that is, only when the
source variable's struct storage was actually eliminated. If the source still
has its own storage (e.g. an IO port), we use the source's own name (see
_Pitfall 3_ below).

### 4. Struct Emission Skip in `V3EmitCHeaders` (src/V3EmitCHeaders.cpp)

In `emitDesignVarDecls()`, any var with both `vpiAlias()` and `noCReset()` is
skipped — no struct member is emitted for it.

---

## Key Design Decisions

### `isSigUserRdPublic()` vs `isSigPublic()`

`GateTrivialAliasReduction` uses `isSigUserRdPublic()` to match **only**
signals explicitly annotated by the user (via `public_flat_rw` / `public_flat_rd`
pragmas or `--public-flat-rw`). `isSigPublic()` also matches structurally
public signals like hierarchical block ports — touching those breaks
`t_hier_block` because they are functionally required and not just VPI
decorations.

### Raw `AstVar*` Pointer for `vpiAlias`

The alias chain is stored as a raw `const AstVar*` pointer rather than a name
string. This avoids costly string lookups and leverages the fact that `AstVar`
nodes are stable once created. The pointer is safe as long as the target node
is not deleted by V3Dead — which is why we restrict alias targets to
public/IO variables (see _Pitfall 1_).

### `noCReset` Dual Duty

The existing `noCReset` flag is repurposed to signal "this var has no struct
storage". This avoids adding a new flag to `AstVar` and naturally integrates
with `V3CCtors` (which already skips `noCReset` vars for reset-value
generation).

### Building the Alias Map on `AstVar` (not `AstVarScope`)

`V3Descope` removes all `AstVarScope` nodes before `V3EmitCSyms` runs. The
alias map is therefore built during `visit(AstVar*)`, not
`visit(AstVarScope*)`.

---

## Pitfalls & Bug Fixes

### Pitfall 1: Dangling Pointers from V3Dead

**Problem:** `vpiAlias()` stored a raw pointer to the alias target's `AstVar`.
When the target was a non-public, non-IO internal signal, `V3Dead` deleted it
— leaving the pointer dangling. Manifested as crashes or corruption in
`t_dpi_var` and `t_inst_tree_inl1_pub1`.

**Fix:** Only set `vpiAlias()` when the driver variable satisfies
`isSigPublic() || isIO()`. These signals are immune from V3Dead elimination.

### Pitfall 2: `isSigPublic()` Breaks Hierarchical Blocks

**Problem:** Using `isSigPublic()` as the candidate filter in
`GateTrivialAliasReduction` caused hierarchical block ports to be
alias-reduced. Those ports are structurally required; reducing them broke
`t_hier_block`.

**Fix:** Switched to `isSigUserRdPublic()`, which only matches user-annotated
public signals.

### Pitfall 3: Cross-Scope Address Mismatch

**Problem:** When a module port `clk` and an interface signal `aclk` form an
alias (`assign aclk = clk`), the source (`aclk`) is an IO port and retains its
own struct member. Unconditionally redirecting the `varInsert()` address to
`clk` produced an invalid member expression (using `clk`'s name in `aclk`'s
scope). Failed on `t_vpi_public_depthn_2`.

**Fix:** Only redirect the `varInsert()` address when `noCReset()` is true,
meaning the source var's struct storage was actually eliminated. IO
ports/interface signals that retain storage use their own member name.

### Pitfall 4: V3Descope Deletes VarScope Nodes

**Problem:** Early implementation tried to build the alias map in
`visit(AstVarScope*)`. But `V3Descope` runs before `V3EmitCSyms` and removes
all `AstVarScope` nodes — the visitor was never triggered.

**Fix:** Moved alias-map construction to `visit(AstVar*)`, which survives all
passes.

### Pitfall 5: Alias Targets Must Retain Storage

**Problem:** If the canonical target of an alias chain is also unreferenced and
gets its storage eliminated, the `varInsert()` address points at a
non-existent struct member.

**Fix:** During the post-inline storage scan, follow every alias chain to its
canonical target and insert that target into the referenced-vars set, ensuring
it keeps its struct storage.

---

## Testing

Key regression tests:

| Test                         | Validates                                      |
|------------------------------|-------------------------------------------------|
| `t_aliasing_public`          | Struct reduction (4 115 → 19)                   |
| `t_vpi_alias`                | VPI read through alias (direct `vpiHandle`)     |
| `t_aliasing`                 | Base aliasing functionality                     |
| `t_hier_block`               | Hierarchical block ports not reduced            |
| `t_gate_chained`             | Chained gate optimization not broken            |
| `t_dpi_var`                  | No dangling pointers (non-public targets)       |
| `t_inst_tree_inl1_pub1`      | No dangling pointers (inlined instances)        |
| `t_vpi_public_depthn_2`      | Cross-scope aliases use correct address         |
