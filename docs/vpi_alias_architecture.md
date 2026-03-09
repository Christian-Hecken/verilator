# VPI Alias Optimization -- Architecture & Design Notes

## *** KNOWN LIMITATION -- DIRECT C++ STRUCT ACCESS IS STALE ***

This optimization eliminates the eval assign for alias-reduced signals. The
struct member still exists (for backward compatibility), but it is **never
updated by eval**. This means:

- **VPI access works correctly.** `varInsert` registers the canonical
  variable's address, so `vpi_get_value` / `vpi_put_value` read/write the
  live value.
- **Direct C++ struct access does NOT work.** Reading `rootp->alias_signal`
  returns the reset value (stale). Writing to it has no effect on eval.

This is acceptable for the typical `--public-flat-rw` use case (bulk VPI/DPI
access for co-simulation or debug tooling), where signals are accessed
exclusively through the VPI API. However, any `--public-flat-rw` user who
also accesses struct members directly (e.g. `rootp->some_signal = 1`) will
get incorrect behavior.

### Why not C++ references?

The ideal fix would be to emit alias members as C++ references to the
canonical variable (`Type& alias = canonical;`), so reads and writes go
straight to the live storage. This was not implemented because of three
C++ constraints:

1. **Anonymous structs.** Verilator wraps struct members in anonymous structs
   to work around compiler limits on member counts. C++ does not allow
   reference members inside anonymous structs.
2. **Constructor initialization.** References must be initialized in the
   constructor's member-initializer list, which is emitted in a different
   file (`___024root__0__Slow.cpp`). Threading the alias information across
   emission boundaries adds significant complexity.
3. **Layout / sizeof changes.** A reference is stored as a hidden pointer
   (8 bytes on 64-bit), changing the struct layout and size. This could
   break code that relies on `sizeof` or `offsetof` the model struct.

A future iteration should address this, likely by emitting alias references
outside the anonymous struct blocks and adding initializer-list entries in
the constructor emitter.

---

## Overview

Public signals annotated with `public_flat_rw` / `public_flat_rd` are normally
blocked from gate-level optimization because VPI registration requires a stable
address for each signal. In many designs the vast majority of these signals are
trivial aliases -- simple `assign A = B` wires -- whose eval logic can be
eliminated while redirecting VPI registration to the canonical variable's
address.

The optimization only activates with `--public-flat-rw`.  Per-signal
annotations (`/*verilator public_flat*/`, `.vlt` config) imply the user
accesses struct members directly from C++ and are left untouched (see
_Pitfall 6_).

Result on the reference design (`t_aliasing_public`): eval logic for **4 096**
alias signals eliminated; struct members remain for compatibility.

---

## Architecture

The implementation spans three files and two pipeline stages:

```
V3Gate  -----------------------------------------------
|                                                      |
|  1. GateTrivialAliasReduction       Detect  assign A = B  patterns.
|     (before GateInline)             Set  vpiAlias(B)  on  A.
|                                     Re-enable gate reduction.
|                                                      |
|  2. GateInline::apply()            Inline/substitute the var.
|                                                      |
-------------------------------------------------------
          |
V3EmitCSyms  --  redirects  varInsert()  address to alias target
```

Struct members are **not** eliminated.  The alias var keeps its storage (for
direct C++ access compatibility) but its eval assign is removed by GateInline.
VPI reads go through the canonical variable's storage via `varInsert` redirect.

### 1. `GateTrivialAliasReduction` (src/V3Gate.cpp)

Runs **before** `GateInline`.  Guarded by `v3Global.opt.publicFlatRW()`.
Scans the gate graph for public variables that were blocked from reduction.
For each candidate:

1. Must have exactly **one driver** (single incoming edge).
2. The driver must be a simple assignment whose RHS is a **pure `AstVarRef`**
   (no expressions, no concatenations -- just `assign A = B`).
3. The driver variable (`B`) must be **public or IO** (see _Pitfall 1_).
4. Source and driver must share the **same scope** (see _Pitfall 3_).

When matched, `vpiAlias(driverVarp)` is set on the source variable and its
graph vertex is re-enabled for substitution (`setReducible("TrivialAlias")`).

### 2. Address Redirection in `V3EmitCSyms` (src/V3EmitCSyms.cpp)

During `visit(AstVar*)`, alias chains are followed to build `m_aliasMap`
(source -> canonical target).

When emitting `varInsert()` calls, the alias target's name is used in the
address expression so VPI reads return the live value from the canonical
variable's storage.

---

## Key Design Decisions

### `--public-flat-rw` Guard

The optimization only runs when `--public-flat-rw` is active.  With that flag,
all signals become public for VPI access.  Users are not expected to access
individual struct members from C++.  Per-signal annotations
(`/*verilator public_flat*/`, `.vlt` config) imply the user intends direct
C++ struct access, so the eval assign must be kept to update the member.

### `isSigUserRdPublic()` vs `isSigPublic()`

`GateTrivialAliasReduction` uses `isSigUserRdPublic()` to match **only**
signals explicitly annotated by the user (via `public_flat_rw` / `public_flat_rd`
pragmas or `--public-flat-rw`). `isSigPublic()` also matches structurally
public signals like hierarchical block ports -- touching those breaks
`t_hier_block` because they are functionally required and not just VPI
decorations.

### Raw `AstVar*` Pointer for `vpiAlias`

The alias chain is stored as a raw `const AstVar*` pointer rather than a name
string. This avoids costly string lookups and leverages the fact that `AstVar`
nodes are stable once created. The pointer is safe as long as the target node
is not deleted by V3Dead -- which is why we restrict alias targets to
public/IO variables (see _Pitfall 1_).

### Building the Alias Map on `AstVar` (not `AstVarScope`)

`V3Descope` removes all `AstVarScope` nodes before `V3EmitCSyms` runs. The
alias map is therefore built during `visit(AstVar*)`, not
`visit(AstVarScope*)`.

---

## Pitfalls & Bug Fixes

### Pitfall 1: Dangling Pointers from V3Dead

**Problem:** `vpiAlias()` stored a raw pointer to the alias target's `AstVar`.
When the target was a non-public, non-IO internal signal, `V3Dead` deleted it
-- leaving the pointer dangling. Manifested as crashes or corruption in
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
alias (`assign aclk = clk`), redirecting the `varInsert()` address to `clk`
produced an invalid member expression -- `clk` doesn't exist as a member of
the interface's struct. Failed on `t_vpi_public_depthn_2`.

**Fix:** In `GateTrivialAliasReduction`, require source and driver to share the
same `AstScope` (`vVtxp->varScp()->scopep() == driverVscp->scopep()`).
Cross-scope aliases are skipped entirely.

### Pitfall 4: V3Descope Deletes VarScope Nodes

**Problem:** Early implementation tried to build the alias map in
`visit(AstVarScope*)`. But `V3Descope` runs before `V3EmitCSyms` and removes
all `AstVarScope` nodes -- the visitor was never triggered.

**Fix:** Moved alias-map construction to `visit(AstVar*)`, which survives all
passes.

### Pitfall 5: Per-Signal `public_flat` Implies Direct C++ Access

**Problem:** When signals are made public via inline annotations
(`/*verilator public_flat*/`) or `.vlt` config (`public -module ... -var ...`),
users access them directly from C++ as struct members
(`rootp->mod__DOT__sig`). Alias-reducing those signals eliminates the eval
assign, leaving the struct member with stale values. Manifested as wrong
values in `t_export_packed_struct2` and a stats mismatch in
`t_inst_tree_inl0_pub1`.

**Fix:** Guard `GateTrivialAliasReduction::analyze()` with
`v3Global.opt.publicFlatRW()`. The optimization only runs when
`--public-flat-rw` is active (all signals public for VPI). Per-signal
annotations imply intentional C++ access and are left untouched.

---

## Testing

Key regression tests:

| Test                         | Validates                                      |
|------------------------------|-------------------------------------------------|
| `t_aliasing_public`          | Eval logic elimination with `--public-flat-rw`  |
| `t_vpi_alias`                | VPI read through alias (direct `vpiHandle`)     |
| `t_aliasing`                 | Base aliasing functionality                     |
| `t_hier_block`               | Hierarchical block ports not reduced            |
| `t_gate_chained`             | Chained gate optimization not broken            |
| `t_dpi_var`                  | No dangling pointers (non-public targets)       |
| `t_inst_tree_inl1_pub1`      | No dangling pointers (inlined instances)        |
| `t_vpi_public_depthn_2`      | Cross-scope aliases skipped                     |
| `t_export_packed_struct2`    | Per-signal `public_flat` retains eval logic     |
| `t_inst_tree_inl0_pub1`      | `.vlt` public signals not alias-reduced         |
